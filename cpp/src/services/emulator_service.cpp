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

// Per-dylib version sidecar for libretro cores. All libretro manifests
// share install_folder "libretro", so a shared .version.json would let
// installing core A stamp core B's version. Empty for non-libretro
// backends (and for malformed libretro manifests without a core_dylib).
static QString libretroVersionSidecarPath(const EmulatorManifest& manifest) {
    if (manifest.backend != QLatin1String("libretro") || manifest.core_dylib.isEmpty())
        return {};
    return Paths::emulatorsDir(manifest.install_folder) + "/cores/"
           + manifest.core_dylib + ".version";
}

static QJsonObject readJsonObjectFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.object();  // empty for non-JSON / non-object content
}

QJsonObject EmulatorService::readVersionRecord(const QString& emuId) const {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return {};

    const QString legacyPath =
        Paths::emulatorsDir(manifest->install_folder) + "/.version.json";

    const QString sidecarPath = libretroVersionSidecarPath(*manifest);
    if (sidecarPath.isEmpty()) {
        // Process backend: install_folder is private to this emulator, so
        // the shared-file hazard doesn't exist. Unchanged behavior.
        return readJsonObjectFile(legacyPath);
    }

    // Libretro: per-dylib sidecar is the source of truth. Older installers
    // wrote the sidecar as a bare published_at string — that fails JSON
    // object parsing and falls through to the legacy record, same as a
    // missing sidecar.
    QJsonObject record = readJsonObjectFile(sidecarPath);
    if (record.contains("version")) return record;

    // Migration fallback: legacy shared .version.json (whichever core was
    // installed last stamped it). Better than showing "not installed"; the
    // per-dylib sidecar gets written on the next install of this core.
    QJsonObject legacy = readJsonObjectFile(legacyPath);
    if (!legacy.isEmpty() && !m_legacyVersionWarned.contains(emuId)) {
        m_legacyVersionWarned.insert(emuId);
        qWarning() << "[EmulatorService]" << emuId
                   << ": no per-core version record at" << sidecarPath
                   << "— falling back to legacy shared .version.json"
                   << "(may show another core's version; reinstall/update to migrate)";
    }
    return legacy;
}

QString EmulatorService::installedVersion(const QString& emuId) const {
    return readVersionRecord(emuId).value("version").toString();
}

QString EmulatorService::installedPublishedAt(const QString& emuId) const {
    return readVersionRecord(emuId).value("published_at").toString();
}

QString EmulatorService::installedAt(const QString& emuId) const {
    return readVersionRecord(emuId).value("installed_at").toString();
}

void EmulatorService::saveVersion(const QString& emuId, const QString& version,
                                  const QString& publishedAt) {
    const EmulatorManifest* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return;

    QJsonObject obj;
    obj["version"] = version;
    obj["published_at"] = publishedAt;
    obj["installed_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Libretro cores → per-dylib sidecar; process emulators → .version.json
    // in their private install dir.
    QString recordPath = libretroVersionSidecarPath(*manifest);
    if (recordPath.isEmpty())
        recordPath = Paths::emulatorsDir(manifest->install_folder) + "/.version.json";
    QDir().mkpath(QFileInfo(recordPath).absolutePath());

    QFile file(recordPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    } else {
        qWarning() << "[EmulatorService] Failed to save version file:" << file.fileName();
    }

    // Remove any stale "update available" entry from the cache so the next
    // app launch doesn't replay the toast for an emulator we just updated.
    const QString cacheFile = Paths::root() + "/update_check.json";
    QFile cache(cacheFile);
    if (!cache.open(QIODevice::ReadOnly)) return;
    QJsonObject cacheObj = QJsonDocument::fromJson(cache.readAll()).object();
    cache.close();
    QJsonObject updates = cacheObj["updates"].toObject();
    if (!updates.contains(emuId)) return;
    updates.remove(emuId);
    cacheObj["updates"] = updates;
    if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cache.write(QJsonDocument(cacheObj).toJson(QJsonDocument::Compact));
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
        saveVersion(emuId, result.version, result.publishedAt);
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
                    saveVersion(emuId, result.version, result.publishedAt);
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
    // Bump when the update-check logic changes in a way that invalidates
    // existing caches (e.g., we switch comparison keys). Caches missing or
    // mismatching this value are ignored.
    constexpr int kCacheSchemaVersion = 2;

    // Rate limit: only check once per day
    const QString cacheFile = Paths::root() + "/update_check.json";
    QFile cache(cacheFile);
    if (cache.open(QIODevice::ReadOnly)) {
        const QJsonObject cacheObj = QJsonDocument::fromJson(cache.readAll()).object();
        cache.close();
        const QDateTime lastTime = QDateTime::fromString(
            cacheObj["last_check"].toString(), Qt::ISODate);
        const int schema = cacheObj.value("schema_version").toInt(0);
        if (schema == kCacheSchemaVersion && lastTime.isValid() &&
            lastTime.secsTo(QDateTime::currentDateTimeUtc()) < 86400) {
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
        QString currentVersion;       // display tag
        QString currentPublishedAt;   // may be empty for legacy installs
        QString currentInstalledAt;   // legacy-install fallback: compare against latest.publishedAt
    };

    QVector<CheckItem> items;
    for (const auto& emu : m_loader->allEmulators()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emu.id);
        if (!adapter || !adapter->isInstalled(emu)) continue;

        // No github_repo → nowhere to check. This is a deliberate state, not
        // a config bug: duckstation's manifest omits github_repo because its
        // license forbids redistribution, so the core is only ever built and
        // deployed locally (see its package.sh). Same for any manifest with
        // neither github_repo nor core_buildbot_path (e.g. mgba after the
        // buildbot path was removed to protect the local universal build).
        if (emu.github_repo.isEmpty()) {
            qInfo() << "[EmulatorService]" << emu.id
                    << ": no distribution source configured — update checks skipped"
                    << "(deliberate for duckstation: license)";
            continue;
        }

        QString version = installedVersion(emu.id);
        if (version.isEmpty()) continue;

        items.append({emu.id, emu.github_repo, version,
                      installedPublishedAt(emu.id), installedAt(emu.id)});
    }

    if (items.isEmpty()) return;

    // Check on background thread — use QPointer guard to avoid dangling this
    QPointer<EmulatorService> guard(this);
    (void)QtConcurrent::run([guard, items, cacheFile]() {
        QJsonObject updates;

        for (const auto& item : items) {
            GitHubClient::LatestRelease latest = GitHubClient::fetchLatestRelease(item.githubRepo);
            if (latest.tag.isEmpty()) continue;

            // Prefer publishedAt comparison — handles rolling tags like DuckStation's
            // "latest" where the tag never changes between releases.
            // For legacy installs without publishedAt, compare the release's publishedAt
            // against when we installed. Falls back to tag comparison if nothing else.
            bool isUpdate = false;
            if (!item.currentPublishedAt.isEmpty() && !latest.publishedAt.isEmpty()) {
                isUpdate = latest.publishedAt != item.currentPublishedAt;
            } else if (!latest.publishedAt.isEmpty() && !item.currentInstalledAt.isEmpty()) {
                const QDateTime released = QDateTime::fromString(latest.publishedAt, Qt::ISODate);
                const QDateTime installed = QDateTime::fromString(item.currentInstalledAt, Qt::ISODate);
                isUpdate = released.isValid() && installed.isValid() && released > installed;
            } else {
                isUpdate = latest.tag != item.currentVersion;
            }
            if (!isUpdate) continue;

            QJsonObject u;
            u["current"] = item.currentVersion;
            u["latest"] = latest.tag;
            updates[item.emuId] = u;

            if (!guard) return;  // Service was destroyed
            QMetaObject::invokeMethod(guard.data(), [guard, emuId = item.emuId,
                                              current = item.currentVersion, latestTag = latest.tag]() {
                if (guard) emit guard->updateAvailable(emuId, current, latestTag);
            }, Qt::QueuedConnection);
        }

        // Save cache
        QJsonObject cacheObj;
        cacheObj["schema_version"] = kCacheSchemaVersion;
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
