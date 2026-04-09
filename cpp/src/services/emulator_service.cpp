#include "emulator_service.h"
#include "core/github_client.h"
#include "core/paths.h"
#include "adapters/adapter_registry.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QPointer>
#include <QCoreApplication>

EmulatorService::EmulatorService(ManifestLoader* loader, QObject* parent)
    : QObject(parent), m_loader(loader) {}

// ── Helpers ───────────────────────────────────────────────

void EmulatorService::ensureConfig(const QString& emuId, const EmulatorManifest& manifest) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);
    const QString biosDir = Paths::biosDir();
    const QString dataDir = Paths::emulatorDataDir(emuId, systemId);
    QDir().mkpath(dataDir);

    // Create all directories defined by the adapter's path definitions.
    // Every non-BIOS entry resolves under this emulator's unified data root.
    for (const auto& pd : adapter->pathsDefs()) {
        QString dir;
        switch (pd.base) {
            case PathBase::Bios:
                dir = biosDir;
                break;
            case PathBase::EmulatorData:
                dir = dataDir + "/" + pd.defaultSuffix;
                break;
        }
        if (!dir.isEmpty()) QDir().mkpath(dir);
    }

    adapter->ensureConfig(manifest,
        QFileInfo(biosDir).absoluteFilePath(),
        QFileInfo(dataDir).absoluteFilePath());
}

// ── Version Tracking ──────────────────────────────────────

QString EmulatorService::installedVersion(const QString& emuId) const {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return {};

    const QString versionPath = Paths::emulatorsDir(manifest->install_folder) + "/.version.json";
    QFile file(versionPath);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.object().value("version").toString();
}

void EmulatorService::saveVersion(const QString& emuId, const QString& version) {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return;

    const QString installDir = Paths::emulatorsDir(manifest->install_folder);
    QDir().mkpath(installDir);

    QJsonObject obj;
    obj["version"] = version;
    obj["installed_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QFile file(installDir + "/.version.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } else {
        qWarning() << "[EmulatorService] Failed to save version file:" << file.fileName();
    }
}

// ── Synchronous Install (CLI) ─────────────────────────────

EmulatorService::InstallResult EmulatorService::installEmulatorSync(const QString& emuId) {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest)
        return {false, "No manifest for: " + emuId, {}};

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter)
        return {false, "No adapter for: " + emuId, {}};

    emit statusMessage("Installing " + manifest->name + "...");

    const QString installPath = Paths::emulatorsDir(manifest->install_folder);
    auto result = EmulatorInstaller::installSync(*manifest, installPath);

    if (result.success) {
        ensureConfig(emuId, *manifest);
        saveVersion(emuId, result.version);
    }

    return result;
}

// ── Async Install (GUI) ───────────────────────────────────

void EmulatorService::installEmulatorAsync(const QString& emuId) {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) {
        emit installFinished(emuId, false, "No manifest for: " + emuId);
        return;
    }

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) {
        emit installFinished(emuId, false, "No adapter for: " + emuId);
        return;
    }

    emit statusMessage("Installing " + manifest->name + "...");

    const QString installPath = Paths::emulatorsDir(manifest->install_folder);

    // Create installer on heap — cleaned up after finished signal
    auto* installer = new EmulatorInstaller(this);

    connect(installer, &EmulatorInstaller::progress, this,
        [this, emuId](double ratio, const QString& phase, const QString& detail) {
            emit installProgress(emuId, ratio, phase, detail);
        });

    connect(installer, &EmulatorInstaller::finished, this,
        [this, emuId, installer](EmulatorInstaller::InstallResult result) {
            if (result.success) {
                const EmulatorManifest* m = m_loader->emulatorById(emuId);
                if (m) {
                    ensureConfig(emuId, *m);
                    saveVersion(emuId, result.version);
                }
            }
            emit installFinished(emuId, result.success, result.message);
            installer->deleteLater();
        });

    installer->installAsync(*manifest, installPath);
}

// ── Async Uninstall ───────────────────────────────────────

void EmulatorService::uninstallEmulator(const QString& emuId) {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) {
        emit uninstallFinished(emuId, false, "Unknown emulator: " + emuId);
        return;
    }

    const QString installDir = Paths::emulatorsDir(manifest->install_folder);
    if (!QDir(installDir).exists()) {
        emit uninstallFinished(emuId, false, manifest->name + " is not installed.");
        return;
    }

    const QString name = manifest->name;

    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this,
        [this, emuId, name, watcher]() {
            bool ok = watcher->result();
            if (ok)
                emit uninstallFinished(emuId, true, name + " has been uninstalled.");
            else
                emit uninstallFinished(emuId, false, "Failed to uninstall " + name + ".");
            watcher->deleteLater();
        });

    watcher->setFuture(QtConcurrent::run([installDir]() {
        return QDir(installDir).removeRecursively();
    }));
}

// ── Update Check ─────────────────────────────────────────

void EmulatorService::checkForUpdates() {
    // Rate limit: only check once per day
    const QString cacheFile = Paths::root() + "/update_check.json";
    QFile cache(cacheFile);
    if (cache.open(QIODevice::ReadOnly)) {
        const QJsonObject cacheObj = QJsonDocument::fromJson(cache.readAll()).object();
        cache.close();
        const QDateTime lastTime = QDateTime::fromString(
            cacheObj["last_check"].toString(), Qt::ISODate);
        if (lastTime.isValid() && lastTime.secsTo(QDateTime::currentDateTimeUtc()) < 86400) {
            // Use cached results
            const QJsonObject updates = cacheObj["updates"].toObject();
            for (auto it = updates.begin(); it != updates.end(); ++it) {
                const QJsonObject u = it.value().toObject();
                emit updateAvailable(it.key(), u["current"].toString(), u["latest"].toString());
            }
            return;
        }
    }

    // Collect installed emulator info on main thread
    struct CheckItem {
        QString emuId;
        QString githubRepo;
        QString currentVersion;
    };

    QVector<CheckItem> items;
    for (const auto& emu : m_loader->allEmulators()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emu.id);
        if (!adapter || !adapter->isInstalled(emu)) continue;

        QString version = installedVersion(emu.id);
        if (version.isEmpty()) continue;

        items.append({emu.id, emu.github_repo, version});
    }

    if (items.isEmpty()) return;

    // Check on background thread — use QPointer guard to avoid dangling this
    QPointer<EmulatorService> guard(this);
    (void)QtConcurrent::run([guard, items, cacheFile]() {
        QJsonObject updates;

        for (const auto& item : items) {
            QString latestTag = GitHubClient::fetchLatestTag(item.githubRepo);
            if (latestTag.isEmpty() || latestTag == item.currentVersion) continue;

            QJsonObject u;
            u["current"] = item.currentVersion;
            u["latest"] = latestTag;
            updates[item.emuId] = u;

            if (!guard) return;  // Service was destroyed
            QMetaObject::invokeMethod(guard.data(), [guard, emuId = item.emuId,
                                              current = item.currentVersion, latestTag]() {
                if (guard) emit guard->updateAvailable(emuId, current, latestTag);
            }, Qt::QueuedConnection);
        }

        // Save cache
        QJsonObject cacheObj;
        cacheObj["last_check"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        cacheObj["updates"] = updates;

        // Cache write doesn't need the service, just write the file
        QMetaObject::invokeMethod(qApp, [cacheFile, cacheObj]() {
            QFile file(cacheFile);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(cacheObj).toJson(QJsonDocument::Compact));
            } else {
                qWarning() << "[EmulatorService] Failed to write update cache:" << cacheFile;
            }
        }, Qt::QueuedConnection);
    });
}
