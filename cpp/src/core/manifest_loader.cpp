#include "manifest_loader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

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

        EmulatorManifest m;
        m.id = obj["id"].toString();
        m.name = obj["name"].toString();
        m.description = obj["description"].toString();
        m.systems = jsonArrayToStringList(obj["systems"].toArray());
        m.github_repo = obj["github_repo"].toString();
        m.executable = obj["executable"].toString();
        m.install_folder = obj["install_folder"].toString();
        m.rom_extensions = jsonArrayToStringList(obj["rom_extensions"].toArray());
        m.launch_args = jsonArrayToStringList(obj["launch_args"].toArray());

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
    if (m.github_repo.isEmpty()) missing << "github_repo";
    if (m.executable.isEmpty()) missing << "executable";

    if (!missing.isEmpty()) {
        qWarning() << "Manifest" << filePath << "missing required fields:" << missing.join(", ");
        return false;
    }

    return true;
}
