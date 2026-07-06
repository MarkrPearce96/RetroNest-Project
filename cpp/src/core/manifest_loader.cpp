#include "manifest_loader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSet>

static QStringList jsonArrayToStringList(const QJsonArray& arr) {
    QStringList result;
    for (const auto& v : arr) {
        if (v.isString()) {
            result.append(v.toString());
        }
    }
    return result;
}

bool ManifestLoader::loadAll(const QString& manifestsDir) {
    m_manifests.clear();
    m_idIndex.clear();

    QDir dir(manifestsDir);
    if (!dir.exists()) {
        qWarning() << "Manifests directory does not exist:" << manifestsDir;
        return false;
    }

    const auto files = dir.entryList({"*.json"}, QDir::Files);
    if (files.isEmpty()) {
        qWarning() << "No manifest files found in:" << manifestsDir;
        return false;
    }

    for (const auto& filename : files) {
        if (filename == QLatin1String("systems.json"))
            continue;   // system registry, not an emulator manifest (SystemRegistry::load)
        const QString filePath = dir.absoluteFilePath(filename);
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open manifest file:" << filePath;
            continue;
        }

        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error in" << filePath << ":" << parseError.errorString();
            continue;
        }

        if (!doc.isObject()) {
            qWarning() << "Manifest is not a JSON object:" << filePath;
            continue;
        }

        const auto obj = doc.object();

        // Loader hardening (packet 7 stage 3): version stamp + typo net.
        static const QSet<QString> kKnownKeys = {
            "manifest_version", "id", "name", "description", "systems",
            "github_repo", "executable", "install_folder", "rom_extensions",
            "launch_args", "backend", "core_dylib", "core_buildbot_path",
            "core_arch", "logo", "detail_page",
        };
        for (const QString& key : obj.keys()) {
            if (!kKnownKeys.contains(key))
                qWarning() << "[Manifest]" << filePath << "unknown key" << key
                           << "— ignored (typo?)";
        }

        EmulatorManifest m;
        m.manifest_version = obj.value("manifest_version").toInt(0);
        if (m.manifest_version == 0)
            qWarning() << "[Manifest]" << filePath << "missing manifest_version — treating as v0";
        else if (m.manifest_version > 1)
            qWarning() << "[Manifest]" << filePath << "manifest_version" << m.manifest_version
                       << "is newer than this build understands (1)";
        m.id = obj["id"].toString();
        m.name = obj["name"].toString();
        m.description = obj["description"].toString();
        m.systems = jsonArrayToStringList(obj["systems"].toArray());
        m.github_repo = obj["github_repo"].toString();
        m.executable = obj["executable"].toString();
        m.install_folder = obj["install_folder"].toString();
        m.rom_extensions = jsonArrayToStringList(obj["rom_extensions"].toArray());
        m.launch_args = jsonArrayToStringList(obj["launch_args"].toArray());
        m.backend = obj.value("backend").toString();  // must be "libretro" (validated below)
        m.core_dylib = obj.value("core_dylib").toString();
        m.core_buildbot_path = obj.value("core_buildbot_path").toString();

        // Optional declared binary architecture of the distributed core.
        // Drives the arch-mismatch warning toast (GameSession) and the
        // verify-universal.sh gate. Unknown values degrade to "undeclared".
        m.core_arch = obj.value("core_arch").toString();
        if (!m.core_arch.isEmpty() &&
            m.core_arch != QLatin1String("universal") &&
            m.core_arch != QLatin1String("x86_64") &&
            m.core_arch != QLatin1String("arm64")) {
            qWarning() << "[Manifest]" << filePath << "has invalid core_arch"
                       << m.core_arch << "— expected universal|x86_64|arm64; treating as undeclared";
            m.core_arch.clear();
        }

        // Packet 7 stage 3: UI-facing capability fields.
        m.logo = obj.value("logo").toString();
        const QJsonObject dp = obj.value("detail_page").toObject();
        m.has_patches = dp.value("has_patches").toBool(false);
        for (const auto& v : dp.value("controller_pages").toArray()) {
            const QJsonObject po = v.toObject();
            ManifestControllerPage page;
            page.label = po.value("label").toString();
            page.type  = po.value("type").toString();
            if (!page.label.isEmpty())
                m.controller_pages.append(page);
        }

        // Default install_folder to id if not specified
        if (m.install_folder.isEmpty()) {
            m.install_folder = m.id;
        }

        if (!validateManifest(m, filePath)) {
            continue;
        }

        // Check for duplicate IDs (m_idIndex is the authoritative set of loaded IDs)
        if (m_idIndex.contains(m.id)) {
            qWarning() << "Duplicate manifest id" << m.id << "in" << filePath << "- skipping";
            continue;
        }

        const int index = m_manifests.size();
        m_manifests.append(m);
        m_idIndex.insert(m.id, index);
    }

    qInfo() << "Loaded" << m_manifests.size() << "emulator manifest(s)";
    return !m_manifests.isEmpty();
}

const EmulatorManifest* ManifestLoader::emulatorById(const QString& id) const {
    auto it = m_idIndex.find(id);
    if (it == m_idIndex.end()) {
        return nullptr;
    }
    return &m_manifests[it.value()];
}

const QVector<EmulatorManifest>& ManifestLoader::allEmulators() const {
    return m_manifests;
}

bool ManifestLoader::validateManifest(const EmulatorManifest& m, const QString& filePath) const {
    QStringList missing;

    if (m.id.isEmpty()) missing << "id";
    if (m.name.isEmpty()) missing << "name";
    if (m.systems.isEmpty()) missing << "systems";
    if (m.executable.isEmpty()) missing << "executable";

    if (!missing.isEmpty()) {
        qWarning() << "Manifest" << filePath << "missing required fields:" << missing.join(", ");
        return false;
    }

    // Process-era retirement (2026-07): every emulator is an in-process
    // libretro core. A manifest that declares anything else — or nothing —
    // is rejected loudly instead of silently defaulting to a launch path
    // that no longer exists. (Cores may be local-only: github_repo stays
    // optional — the duckstation licensing pattern.)
    if (m.backend != QLatin1String("libretro")) {
        qWarning() << "[Manifest] Rejecting" << m.id << "— unsupported backend"
                   << (m.backend.isEmpty() ? QStringLiteral("<missing>") : m.backend)
                   << "(process-backend emulators were retired; only libretro cores load)";
        return false;
    }

    return true;
}
