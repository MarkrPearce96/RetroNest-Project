# Emulator Management Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign emulator management to support the full lifecycle — discovery, install with real progress, configuration, reset, update checks, and uninstall — all with progress popups and startup update notifications.

**Architecture:** The installer moves from synchronous blocking to async with `QNetworkAccessManager` download progress signals piped through `EmulatorService` → `AppController` → QML. Version metadata is persisted in `.version.json` alongside each install. A startup update checker compares installed versions against GitHub releases (rate-limited to once/day). QML gets three new components: `ProgressPopup`, `UpdateNotification`, and a reset confirmation dialog.

**Tech Stack:** C++17, Qt6 (QML + Widgets + Network + Concurrent), CMake

---

## File Structure

| File | Responsibility |
|------|---------------|
| `cpp/src/core/emulator_installer.h/cpp` | Async download with progress signals, version tag extraction |
| `cpp/src/services/emulator_service.h/cpp` | Async install/uninstall orchestration, update checking |
| `cpp/src/ui/app_controller.h/cpp` | New signals for QML, enhanced resetConfiguration |
| `cpp/qml/AppUI/EmulatorManageGrid.qml` | Show all emulators (installed + uninstalled) |
| `cpp/qml/AppUI/EmulatorDetailPage.qml` | Two states (install view / full actions), version display, reset confirmation |
| `cpp/qml/AppUI/ProgressPopup.qml` | **New** — reusable modal progress popup |
| `cpp/qml/AppUI/UpdateNotification.qml` | **New** — startup update toast |
| `cpp/qml/AppUI/AppWindow.qml` | Add UpdateNotification, wire update signals |
| `cpp/qml/AppUI/SettingsOverlay.qml` | Add navigateToEmulator function |
| `cpp/CMakeLists.txt` | Register new QML files |

---

### Task 1: Async Installer with Progress Signals

**Files:**
- Modify: `cpp/src/core/emulator_installer.h`
- Modify: `cpp/src/core/emulator_installer.cpp`

This task converts the synchronous installer to async with download progress reporting and version tag extraction.

- [ ] **Step 1: Update EmulatorInstaller header with new return type and async API**

Replace the entire header with:

```cpp
#pragma once

#include "core/manifest.h"
#include <QObject>
#include <QString>
#include <QStringList>

/**
 * EmulatorInstaller — downloads and extracts emulators from GitHub releases.
 * Now async with progress signals.
 */
class EmulatorInstaller : public QObject {
    Q_OBJECT

public:
    explicit EmulatorInstaller(QObject* parent = nullptr);

    struct InstallResult {
        bool success = false;
        QString message;
        QString version;  // GitHub release tag, e.g. "v1.7.5"
    };

    /** Start async install. Emits progress/finished signals. */
    void installAsync(const EmulatorManifest& manifest, const QString& installPath);

    /** Synchronous install (kept for CLI mode). */
    static InstallResult installSync(const EmulatorManifest& manifest, const QString& installPath);

signals:
    /** progress: 0.0–1.0 for download, -1 for indeterminate (extract phase) */
    void progress(double progress, const QString& phase, const QString& detail);
    void finished(const InstallResult& result);

private:
    static QString matchAsset(const QString& emuId, const QStringList& assetNames);
    static bool extract(const QString& archivePath, const QString& destPath);
};
```

- [ ] **Step 2: Implement the async installer in the .cpp file**

Replace the entire `emulator_installer.cpp` with the async implementation. Key changes from the existing code:

1. `installSync()` is the old `install()` but now returns `InstallResult` with `version` populated from `tag_name`
2. `installAsync()` runs `installSync()` on a background thread via `QtConcurrent::run`, but uses `QNetworkAccessManager` on the main thread for download progress
3. The download function is split: async version emits `downloadProgress` signals, sync version is kept for CLI

```cpp
#include "emulator_installer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QDebug>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QThread>
#include <QtConcurrent>

EmulatorInstaller::EmulatorInstaller(QObject* parent)
    : QObject(parent) {}

// ============================================================================
// HTTP helpers (synchronous via local event loop)
// ============================================================================

static QByteArray httpGet(const QString& url) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "EmulatorFrontend/1.0");
    req.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Installer] HTTP error:" << reply->errorString();
        reply->deleteLater();
        return {};
    }

    QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (redirect.isValid()) {
        reply->deleteLater();
        return httpGet(redirect.toUrl().toString());
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

// ============================================================================
// Synchronous download (for CLI mode / extract helper)
// ============================================================================

static bool downloadSync(const QString& url, const QString& destPath) {
    qInfo() << "[Installer] Downloading" << url;

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "EmulatorFrontend/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Installer] Download failed:" << reply->errorString();
        reply->deleteLater();
        return false;
    }

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[Installer] Cannot write to" << destPath;
        reply->deleteLater();
        return false;
    }

    file.write(reply->readAll());
    file.close();
    reply->deleteLater();

    qInfo() << "[Installer] Downloaded to" << destPath
            << "(" << QFileInfo(destPath).size() / (1024 * 1024) << "MB)";
    return true;
}

// ============================================================================
// Extract archive
// ============================================================================

bool EmulatorInstaller::extract(const QString& archivePath, const QString& destPath) {
    QDir().mkpath(destPath);
    const QString absArchive = QFileInfo(archivePath).absoluteFilePath();
    const QString absDest = QFileInfo(destPath).absoluteFilePath();
    const QString name = absArchive.toLower();

    QProcess proc;
    proc.setWorkingDirectory(absDest);

    if (name.endsWith(".zip")) {
        proc.start("unzip", {"-o", absArchive, "-d", absDest});
    } else if (name.endsWith(".tar.xz") || name.endsWith(".tar.gz")) {
        proc.start("tar", {"-xf", absArchive, "-C", absDest});
    } else if (name.endsWith(".dmg")) {
        QTemporaryDir mountPoint;
        if (!mountPoint.isValid()) return false;

        QProcess mount;
        mount.start("hdiutil", {"attach", absArchive, "-mountpoint",
                                mountPoint.path(), "-nobrowse", "-quiet"});
        mount.waitForFinished(60000);
        if (mount.exitCode() != 0) {
            qWarning() << "[Installer] hdiutil attach failed:" << mount.readAllStandardError();
            return false;
        }

        QDir mountDir(mountPoint.path());
        auto apps = mountDir.entryList({"*.app"}, QDir::Dirs);
        bool ok = false;
        if (!apps.isEmpty()) {
            const QString src = mountPoint.path() + "/" + apps.first();
            const QString dst = absDest + "/" + apps.first();
            QProcess cp;
            cp.start("cp", {"-R", src, dst});
            cp.waitForFinished(120000);
            ok = (cp.exitCode() == 0);
        }

        QProcess detach;
        detach.start("hdiutil", {"detach", mountPoint.path(), "-quiet"});
        detach.waitForFinished(30000);
        return ok;
    } else if (name.endsWith(".appimage")) {
        const QString dest = absDest + "/" + QFileInfo(absArchive).fileName();
        QFile::copy(archivePath, dest);
        QProcess::execute("chmod", {"755", dest});
        return true;
    } else {
        qWarning() << "[Installer] Unknown archive type:" << archivePath;
        return false;
    }

    proc.waitForFinished(120000);
    if (proc.exitCode() != 0) {
        qWarning() << "[Installer] Extract failed:" << proc.readAllStandardError();
        return false;
    }

    qInfo() << "[Installer] Extracted to" << destPath;
    return true;
}

// ============================================================================
// Asset matching
// ============================================================================

QString EmulatorInstaller::matchAsset(const QString& emuId, const QStringList& assetNames) {
#if defined(Q_OS_MACOS)
    const QString platform = "mac";
#elif defined(Q_OS_WIN)
    const QString platform = "windows";
#else
    const QString platform = "linux";
#endif

    for (const auto& name : assetNames) {
        const QString lower = name.toLower();

        if (emuId == "duckstation") {
#if defined(Q_OS_MACOS)
            if (lower.contains("mac") && name.endsWith(".zip")) return name;
#elif defined(Q_OS_WIN)
            if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip")) return name;
#else
            if (lower.contains("linux") && lower.contains("x64") && name.endsWith(".AppImage")) return name;
#endif
        } else if (emuId == "pcsx2") {
#if defined(Q_OS_MACOS)
            if (lower.contains("mac") && (name.endsWith(".tar.xz") || name.endsWith(".dmg"))) return name;
#elif defined(Q_OS_WIN)
            if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip")) return name;
#else
            if (name.endsWith(".AppImage")) return name;
#endif
        }
    }

    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
        if (lower.contains(platform) &&
            (name.endsWith(".zip") || name.endsWith(".tar.xz") || name.endsWith(".dmg") || name.endsWith(".tar.gz"))) {
            return name;
        }
    }

    return {};
}

// ============================================================================
// Synchronous install (for CLI mode)
// ============================================================================

EmulatorInstaller::InstallResult EmulatorInstaller::installSync(
    const EmulatorManifest& manifest, const QString& installPath)
{
    qInfo() << "[Installer] Installing" << manifest.name << "from" << manifest.github_repo;

    const QString apiUrl = "https://api.github.com/repos/" + manifest.github_repo + "/releases/latest";
    QByteArray releaseJson = httpGet(apiUrl);
    if (releaseJson.isEmpty())
        return {false, "Failed to fetch release info", {}};

    QJsonDocument doc = QJsonDocument::fromJson(releaseJson);
    if (!doc.isObject())
        return {false, "Invalid release JSON", {}};

    QJsonObject release = doc.object();
    QString tagName = release["tag_name"].toString();
    QJsonArray assets = release["assets"].toArray();

    qInfo() << "[Installer] Latest release:" << tagName << "with" << assets.size() << "assets";

    QStringList assetNames;
    QHash<QString, QString> assetUrls;
    for (const auto& a : assets) {
        QJsonObject asset = a.toObject();
        QString name = asset["name"].toString();
        QString url = asset["browser_download_url"].toString();
        assetNames.append(name);
        assetUrls.insert(name, url);
    }

    QString matchedAsset = matchAsset(manifest.id, assetNames);
    if (matchedAsset.isEmpty())
        return {false, "No matching asset for platform. Available: " + assetNames.join(", "), {}};

    QString downloadUrl = assetUrls[matchedAsset];
    qInfo() << "[Installer] Selected asset:" << matchedAsset;

    QDir().mkpath(installPath);
    const QString tempFile = installPath + "/" + matchedAsset;

    if (!downloadSync(downloadUrl, tempFile))
        return {false, "Download failed", {}};

    if (!extract(tempFile, installPath)) {
        QFile::remove(tempFile);
        return {false, "Extraction failed", {}};
    }

    QFile::remove(tempFile);

#if defined(Q_OS_MACOS)
    QProcess::execute("xattr", {"-rd", "com.apple.quarantine", installPath});
#endif

    qInfo() << "[Installer] Successfully installed" << manifest.name << "to" << installPath;
    return {true, manifest.name + " installed successfully.", tagName};
}

// ============================================================================
// Async install (for GUI mode — download progress via QNetworkAccessManager)
// ============================================================================

void EmulatorInstaller::installAsync(const EmulatorManifest& manifest, const QString& installPath) {
    // Phase 1: Fetch release metadata (quick HTTP GET, synchronous is fine)
    emit progress(-1, "Preparing", "Fetching release info...");

    const QString apiUrl = "https://api.github.com/repos/" + manifest.github_repo + "/releases/latest";
    QByteArray releaseJson = httpGet(apiUrl);
    if (releaseJson.isEmpty()) {
        emit finished({false, "Failed to fetch release info", {}});
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(releaseJson);
    if (!doc.isObject()) {
        emit finished({false, "Invalid release JSON", {}});
        return;
    }

    QJsonObject release = doc.object();
    QString tagName = release["tag_name"].toString();
    QJsonArray assets = release["assets"].toArray();

    QStringList assetNames;
    QHash<QString, QString> assetUrls;
    for (const auto& a : assets) {
        QJsonObject asset = a.toObject();
        QString name = asset["name"].toString();
        QString url = asset["browser_download_url"].toString();
        assetNames.append(name);
        assetUrls.insert(name, url);
    }

    QString matchedAsset = matchAsset(manifest.id, assetNames);
    if (matchedAsset.isEmpty()) {
        emit finished({false, "No matching asset for platform", {}});
        return;
    }

    QString downloadUrl = assetUrls[matchedAsset];
    QDir().mkpath(installPath);
    const QString tempFile = installPath + "/" + matchedAsset;

    // Phase 2: Download with progress
    emit progress(0.0, "Downloading", "Starting download...");

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl(downloadUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "EmulatorFrontend/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam->get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) {
            double pct = static_cast<double>(received) / total;
            QString detail = QString("%1% — %2 MB / %3 MB")
                .arg(static_cast<int>(pct * 100))
                .arg(received / (1024 * 1024))
                .arg(total / (1024 * 1024));
            emit progress(pct, "Downloading", detail);
        }
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, nam, tempFile, installPath, tagName, manifest]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit finished({false, "Download failed: " + reply->errorString(), {}});
            return;
        }

        // Write to disk
        QFile file(tempFile);
        if (!file.open(QIODevice::WriteOnly)) {
            emit finished({false, "Cannot write to " + tempFile, {}});
            return;
        }
        file.write(reply->readAll());
        file.close();

        // Phase 3: Extract (on background thread to avoid blocking UI)
        emit progress(-1, "Extracting", "Extracting files...");

        auto future = QtConcurrent::run([this, tempFile, installPath, tagName, manifest]() {
            bool ok = extract(tempFile, installPath);
            QFile::remove(tempFile);

            if (!ok)
                return InstallResult{false, "Extraction failed", {}};

#if defined(Q_OS_MACOS)
            QProcess::execute("xattr", {"-rd", "com.apple.quarantine", installPath});
#endif

            return InstallResult{true, manifest.name + " installed successfully.", tagName};
        });

        // Use a watcher to get the result back on the main thread
        auto* watcher = new QFutureWatcher<InstallResult>(this);
        connect(watcher, &QFutureWatcher<InstallResult>::finished, this,
                [this, watcher]() {
            emit finished(watcher->result());
            watcher->deleteLater();
        });
        watcher->setFuture(future);
    });
}
```

- [ ] **Step 3: Verify the build compiles**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

The existing code that calls `EmulatorInstaller::install()` (in `emulator_service.cpp` and possibly CLI mode in `main.cpp`) will now fail because the old static `install()` is gone. That's expected — we'll fix those call sites in the next tasks.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/emulator_installer.h cpp/src/core/emulator_installer.cpp
git commit -m "feat: async emulator installer with download progress and version tracking"
```

---

### Task 2: Version Tracking (write and read .version.json)

**Files:**
- Modify: `cpp/src/services/emulator_service.h`
- Modify: `cpp/src/services/emulator_service.cpp`
- Modify: `cpp/src/ui/app_controller.cpp` (the `allEmulatorStatus` and `installEmulator` methods)

This task adds version persistence — saving the GitHub release tag on install and reading it back for display.

- [ ] **Step 1: Add version helpers and async signals to EmulatorService**

Replace `cpp/src/services/emulator_service.h` with:

```cpp
#pragma once

#include "core/manifest_loader.h"
#include "core/emulator_installer.h"
#include <QObject>
#include <QString>

/**
 * EmulatorService — orchestrates install, uninstall, and update-check workflows.
 */
class EmulatorService : public QObject {
    Q_OBJECT

public:
    explicit EmulatorService(ManifestLoader* loader, QObject* parent = nullptr);

    /** Synchronous install (for CLI mode). */
    EmulatorInstaller::InstallResult installEmulatorSync(const QString& emuId);

    /** Async install with progress (for GUI mode). */
    void installEmulatorAsync(const QString& emuId);

    /** Async uninstall. */
    void uninstallEmulator(const QString& emuId);

    /** Read installed version from .version.json, or empty string. */
    QString installedVersion(const QString& emuId) const;

    /** Save version to .version.json after successful install. */
    void saveVersion(const QString& emuId, const QString& version);

signals:
    void statusMessage(const QString& msg);
    void installProgress(const QString& emuId, double progress,
                         const QString& phase, const QString& detail);
    void installFinished(const QString& emuId, bool success, const QString& message);
    void uninstallFinished(const QString& emuId, bool success, const QString& message);

private:
    ManifestLoader* m_loader;
};
```

- [ ] **Step 2: Implement EmulatorService with version tracking and async operations**

Replace `cpp/src/services/emulator_service.cpp` with:

```cpp
#include "emulator_service.h"
#include "core/paths.h"
#include "core/emulator_installer.h"
#include "adapters/adapter_registry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QtConcurrent>

EmulatorService::EmulatorService(ManifestLoader* loader, QObject* parent)
    : QObject(parent), m_loader(loader) {}

// ── Version tracking ──────────────────────────────────────

QString EmulatorService::installedVersion(const QString& emuId) const {
    const auto* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return {};

    QString versionFile = Paths::emulatorsDir(manifest->install_folder) + "/.version.json";
    QFile file(versionFile);
    if (!file.open(QIODevice::ReadOnly)) return {};

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.object()["version"].toString();
}

void EmulatorService::saveVersion(const QString& emuId, const QString& version) {
    const auto* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return;

    QString versionFile = Paths::emulatorsDir(manifest->install_folder) + "/.version.json";

    QJsonObject obj;
    obj["version"] = version;
    obj["installed_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QFile file(versionFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}

// ── Synchronous install (CLI mode) ────────────────────────

EmulatorInstaller::InstallResult EmulatorService::installEmulatorSync(const QString& emuId) {
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
        const QString systemId = Paths::systemIdFor(manifest->id, manifest->systems);
        QDir().mkpath(Paths::savesDir(systemId));
        adapter->ensureConfig(*manifest,
            QFileInfo(Paths::biosDir()).absoluteFilePath(),
            QFileInfo(Paths::savesDir(systemId)).absoluteFilePath());
        saveVersion(emuId, result.version);
    }

    return result;
}

// ── Async install (GUI mode) ──────────────────────────────

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

    auto* installer = new EmulatorInstaller(this);

    connect(installer, &EmulatorInstaller::progress, this,
            [this, emuId](double progress, const QString& phase, const QString& detail) {
        emit installProgress(emuId, progress, phase, detail);
    });

    connect(installer, &EmulatorInstaller::finished, this,
            [this, emuId, manifest, adapter, installer](const EmulatorInstaller::InstallResult& result) {
        installer->deleteLater();

        if (result.success) {
            const QString systemId = Paths::systemIdFor(manifest->id, manifest->systems);
            QDir().mkpath(Paths::savesDir(systemId));
            adapter->ensureConfig(*manifest,
                QFileInfo(Paths::biosDir()).absoluteFilePath(),
                QFileInfo(Paths::savesDir(systemId)).absoluteFilePath());
            saveVersion(emuId, result.version);
            emit statusMessage(result.message);
        } else {
            emit statusMessage(result.message);
        }

        emit installFinished(emuId, result.success, result.message);
    });

    installer->installAsync(*manifest, installPath);
}

// ── Async uninstall ───────────────────────────────────────

void EmulatorService::uninstallEmulator(const QString& emuId) {
    const auto* manifest = m_loader->emulatorById(emuId);
    if (!manifest) {
        emit uninstallFinished(emuId, false, "Unknown emulator: " + emuId);
        return;
    }

    QString installDir = Paths::emulatorsDir() + "/" + manifest->install_folder;
    QString emuName = manifest->name;

    if (!QDir(installDir).exists()) {
        emit uninstallFinished(emuId, false, emuName + " is not installed.");
        return;
    }

    emit statusMessage("Uninstalling " + emuName + "...");

    auto future = QtConcurrent::run([installDir]() {
        return QDir(installDir).removeRecursively();
    });

    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this,
            [this, emuId, emuName, watcher]() {
        bool ok = watcher->result();
        watcher->deleteLater();

        QString msg = ok ? emuName + " has been uninstalled."
                         : "Failed to uninstall " + emuName + ".";
        emit statusMessage(msg);
        emit uninstallFinished(emuId, ok, msg);
    });
    watcher->setFuture(future);
}
```

- [ ] **Step 3: Update AppController to use new async service methods**

In `cpp/src/ui/app_controller.h`, add new signals and update method signatures. Add these signals after the existing `emulatorInstalled` signal:

```cpp
    void installProgress(const QString& emuId, double progress,
                         const QString& phase, const QString& detail);
    void installFinished(const QString& emuId, bool success, const QString& message);
    void uninstallFinished(const QString& emuId, bool success, const QString& message);
```

In `cpp/src/ui/app_controller.cpp`, update the constructor to connect service signals:

After the existing `connect(&m_emuService, ...)` line, add:

```cpp
    connect(&m_emuService, &EmulatorService::installProgress,
            this, &AppController::installProgress);
    connect(&m_emuService, &EmulatorService::installFinished,
            this, [this](const QString& emuId, bool success, const QString& message) {
        setStatus(message);
        if (success) emit emulatorInstalled(emuId);
        emit installFinished(emuId, success, message);
    });
    connect(&m_emuService, &EmulatorService::uninstallFinished,
            this, [this](const QString& emuId, bool success, const QString& message) {
        setStatus(message);
        if (success) emit emulatorInstalled(emuId);  // reuse to refresh UI
        emit uninstallFinished(emuId, success, message);
    });
```

Replace the `installEmulator` method:

```cpp
void AppController::installEmulator(const QString& emuId) {
    m_emuService.installEmulatorAsync(emuId);
}
```

Replace the `uninstallEmulator` method:

```cpp
void AppController::uninstallEmulator(const QString& emuId) {
    m_emuService.uninstallEmulator(emuId);
}
```

Update `allEmulatorStatus()` to include version info. After `item["biosDetected"] = biosDetected;`, add:

```cpp
        item["version"] = m_emuService.installedVersion(emu.id);
```

- [ ] **Step 4: Update CLI install call site in main.cpp**

Search `main.cpp` for the existing install call. It currently calls `EmulatorInstaller::install()` or `EmulatorService::installEmulator()`. Update it to use `installEmulatorSync()`:

Find the install block and change the call to:
```cpp
auto result = emuService.installEmulatorSync(emuId);
```

The old `installEmulator` method on EmulatorService no longer exists (replaced by `installEmulatorSync` and `installEmulatorAsync`).

- [ ] **Step 5: Build and verify compilation**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -30
```

Expected: Clean build. Fix any compilation errors.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/services/emulator_service.h cpp/src/services/emulator_service.cpp \
        cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp \
        cpp/src/core/emulator_installer.h cpp/src/core/emulator_installer.cpp \
        cpp/src/main.cpp
git commit -m "feat: async install/uninstall with version tracking via EmulatorService"
```

---

### Task 3: ProgressPopup QML Component

**Files:**
- Create: `cpp/qml/AppUI/ProgressPopup.qml`
- Modify: `cpp/CMakeLists.txt` (add to QML module)

- [ ] **Step 1: Create ProgressPopup.qml**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: progressPopup

    property string title: ""
    property string subtitle: ""
    property real progressValue: -1  // 0.0-1.0 for determinate, -1 for indeterminate
    property string progressText: ""
    property color accentColor: Theme.accent

    anchors.centerIn: parent
    width: 360
    height: contentColumn.height + 56
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: 28

    background: Rectangle {
        radius: 12
        color: Theme.surface
        border.width: 1
        border.color: Theme.divider
    }

    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        Text {
            text: progressPopup.title
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.Bold
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            text: progressPopup.subtitle
            color: Theme.textDim
            font.pixelSize: 13
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            Layout.bottomMargin: 12
        }

        // Progress bar container
        Rectangle {
            Layout.fillWidth: true
            height: 8
            radius: 4
            color: Theme.background
            clip: true

            // Determinate fill
            Rectangle {
                visible: progressPopup.progressValue >= 0
                width: parent.width * Math.max(0, Math.min(1, progressPopup.progressValue))
                height: parent.height
                radius: 4
                color: progressPopup.accentColor

                Behavior on width { NumberAnimation { duration: 150 } }
            }

            // Indeterminate sliding bar
            Rectangle {
                id: indeterminateBar
                visible: progressPopup.progressValue < 0
                width: parent.width * 0.4
                height: parent.height
                radius: 4
                color: progressPopup.accentColor

                SequentialAnimation on x {
                    loops: Animation.Infinite
                    running: indeterminateBar.visible
                    NumberAnimation {
                        from: 0
                        to: indeterminateBar.parent.width * 0.6
                        duration: 1200
                        easing.type: Easing.InOutQuad
                    }
                    NumberAnimation {
                        from: indeterminateBar.parent.width * 0.6
                        to: 0
                        duration: 1200
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        Text {
            text: progressPopup.progressText
            color: Theme.textDim
            font.pixelSize: 11
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            visible: progressPopup.progressText !== ""
        }
    }
}
```

- [ ] **Step 2: Register in CMakeLists.txt**

In `cpp/CMakeLists.txt`, find the `qt_add_qml_module` block for `appui_module` (the one with URI `AppUI`). Add `ProgressPopup.qml` to its QML_FILES list, after `EmulatorDetailPage.qml`:

```
ProgressPopup.qml
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

Expected: Clean build.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/ProgressPopup.qml cpp/CMakeLists.txt
git commit -m "feat: add ProgressPopup QML component for install/uninstall operations"
```

---

### Task 4: Emulator Grid — Show All Emulators

**Files:**
- Modify: `cpp/qml/AppUI/EmulatorManageGrid.qml`

- [ ] **Step 1: Update the grid to show all emulators with installed/uninstalled states**

Replace the entire file:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: root

    signal emulatorSelected(string emuId)

    property var emuList: app.allEmulatorStatus()

    function logoForEmu(emuId) {
        var logos = {
            "pcsx2": "qrc:/AppUI/qml/AppUI/images/pcsx2_logo.png",
            "duckstation": "qrc:/AppUI/qml/AppUI/images/duckstation_logo.png"
        }
        return logos[emuId] || ""
    }

    Connections {
        target: app
        function onEmulatorInstalled() {
            root.emuList = app.allEmulatorStatus()
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: contentCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: parent.width
            spacing: 0

            Text {
                text: "Manage Emulators"
                color: Theme.textPrimary
                font.pixelSize: 22
                font.weight: Font.Bold
                Layout.margins: 28
                Layout.bottomMargin: 4
            }

            Text {
                text: "Select an emulator to configure settings, controller mappings, and hotkeys."
                color: Theme.textDim
                font.pixelSize: 13
                Layout.leftMargin: 28
                Layout.bottomMargin: 24
            }

            // Emulator card grid
            Flow {
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.fillWidth: true
                spacing: 16

                Repeater {
                    model: root.emuList

                    delegate: Rectangle {
                        id: card
                        width: 140
                        height: 140
                        radius: 12
                        color: cardMouse.containsMouse ? Theme.surfaceHover : Theme.surface
                        border.width: 1
                        border.color: cardMouse.containsMouse ? Theme.accent : Theme.divider
                        clip: true
                        opacity: modelData.installed ? 1.0 : 0.45

                        Behavior on color { ColorAnimation { duration: 120 } }
                        Behavior on border.color { ColorAnimation { duration: 120 } }

                        Image {
                            id: logoImg
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: modelData.installed ? 0 : -10
                            width: parent.width - 24
                            height: parent.height - (modelData.installed ? 24 : 40)
                            source: logoForEmu(modelData.id)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            visible: false
                        }

                        Rectangle {
                            id: logoMask
                            anchors.fill: logoImg
                            radius: 10
                            visible: false
                        }

                        OpacityMask {
                            anchors.fill: logoImg
                            source: logoImg
                            maskSource: logoMask
                        }

                        // "Not Installed" badge
                        Rectangle {
                            visible: !modelData.installed
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 8
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: badgeText.width + 16
                            height: 20
                            radius: 4
                            color: Theme.divider

                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: "Not Installed"
                                color: Theme.textDim
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            hoverEnabled: true
                            onClicked: root.emulatorSelected(modelData.id)
                        }

                        scale: cardMouse.pressed ? 0.97 : 1.0
                        Behavior on scale { NumberAnimation { duration: 80 } }
                    }
                }
            }

            Item { height: 28 }
        }
    }
}
```

Key changes from original:
- Removed the `.filter(function(e) { return e.installed })` — shows all emulators
- Added `opacity: modelData.installed ? 1.0 : 0.45`
- Added "Not Installed" badge `Rectangle` for uninstalled emulators
- Adjusted logo vertical offset when badge is shown

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/EmulatorManageGrid.qml
git commit -m "feat: show all emulators in grid with greyed-out uninstalled state"
```

---

### Task 5: Emulator Detail Page — Two States, Version, Reset Confirmation, Progress

**Files:**
- Modify: `cpp/qml/AppUI/EmulatorDetailPage.qml`

This is the largest QML task. The detail page gets:
- Uninstalled state with Install button
- Version display for installed emulators
- Reset confirmation dialog
- Progress popup integration for install/reinstall/uninstall

- [ ] **Step 1: Replace EmulatorDetailPage.qml with the two-state version**

Replace the entire file:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property string emuId: ""
    signal back()

    property var emuList: app.allEmulatorStatus()
    property var emuInfo: {
        for (var i = 0; i < emuList.length; i++) {
            if (emuList[i].id === emuId) return emuList[i]
        }
        return { name: "", systems: "", installed: false, description: "",
                 biosRequired: false, biosDetected: false, version: "" }
    }
    property int _v: 0

    onEmuIdChanged: {
        root.emuList = app.allEmulatorStatus()
        root._v++
    }

    Connections {
        target: app
        function onEmulatorInstalled() {
            root.emuList = app.allEmulatorStatus()
            root._v++
        }
    }

    function logoForEmu(id) {
        var logos = {
            "pcsx2": "qrc:/AppUI/qml/AppUI/images/pcsx2_logo.png",
            "duckstation": "qrc:/AppUI/qml/AppUI/images/duckstation_logo.png"
        }
        return logos[id] || ""
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: 0

        OverviewSection {}
    }

    // ── Progress Popup ────────────────────────────────────
    ProgressPopup {
        id: progressPopup
    }

    Connections {
        target: app
        function onInstallProgress(emuId, progress, phase, detail) {
            if (emuId !== root.emuId) return
            progressPopup.progressValue = progress
            progressPopup.subtitle = phase === "Downloading"
                ? "Downloading latest release..." : "Extracting files..."
            progressPopup.progressText = progress >= 0 ? detail : "Please wait..."
        }
        function onInstallFinished(emuId, success, message) {
            if (emuId !== root.emuId) return
            progressPopup.close()
        }
        function onUninstallFinished(emuId, success, message) {
            if (emuId !== root.emuId) return
            progressPopup.close()
            if (success) root.back()
        }
    }

    // ── Uninstall Confirmation Dialog ─────────────────────
    Popup {
        id: uninstallDialog
        anchors.centerIn: parent
        width: 360
        height: uninstallCol.height + 48
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 12
            color: Theme.surface
            border.width: 1
            border.color: Theme.divider
        }

        ColumnLayout {
            id: uninstallCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            Text {
                text: "Uninstall " + root.emuInfo.name + "?"
                color: Theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            Text {
                text: "This will remove the emulator files. Your games, saves, and BIOS files will not be affected."
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                Button {
                    id: cancelUninstallBtn
                    implicitWidth: 100
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: cancelUninstallBtn.hovered ? Theme.surfaceHover : Theme.surface
                        border.width: 1
                        border.color: Theme.divider
                    }

                    contentItem: Text {
                        text: "Cancel"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: uninstallDialog.close()
                }

                Button {
                    id: confirmUninstallBtn
                    implicitWidth: 120
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: confirmUninstallBtn.hovered ? "#cc3333" : Theme.error
                    }

                    contentItem: Text {
                        text: "Uninstall"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        uninstallDialog.close()
                        progressPopup.title = "Uninstalling " + root.emuInfo.name
                        progressPopup.subtitle = "Removing files..."
                        progressPopup.progressValue = -1
                        progressPopup.progressText = ""
                        progressPopup.accentColor = Theme.error
                        progressPopup.open()
                        app.uninstallEmulator(root.emuId)
                    }
                }
            }
        }
    }

    // ── Reset Configuration Confirmation Dialog ───────────
    Popup {
        id: resetDialog
        anchors.centerIn: parent
        width: 360
        height: resetCol.height + 48
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 12
            color: Theme.surface
            border.width: 1
            border.color: Theme.divider
        }

        ColumnLayout {
            id: resetCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 24
            spacing: 16

            Text {
                text: "Reset " + root.emuInfo.name + " Configuration?"
                color: Theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            Text {
                text: "This will reset all emulator settings, controller mappings, and hotkeys to their install defaults."
                color: Theme.textDim
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                Button {
                    id: cancelResetBtn
                    implicitWidth: 100
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: cancelResetBtn.hovered ? Theme.surfaceHover : Theme.surface
                        border.width: 1
                        border.color: Theme.divider
                    }

                    contentItem: Text {
                        text: "Cancel"
                        color: Theme.textSecondary
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: resetDialog.close()
                }

                Button {
                    id: confirmResetBtn
                    implicitWidth: 120
                    implicitHeight: 36

                    background: Rectangle {
                        radius: 6
                        color: confirmResetBtn.hovered ? Theme.accentLight : Theme.accent
                    }

                    contentItem: Text {
                        text: "Reset"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        app.resetConfiguration(root.emuId)
                        root.emuList = app.allEmulatorStatus()
                        root._v++
                        resetDialog.close()
                    }
                }
            }
        }
    }

    // ── Overview Section ──────────────────────────────────
    component OverviewSection: Flickable {
        contentHeight: overviewCol.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: overviewCol
            width: parent.width
            spacing: 0

            // Back button + title header
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 24
                Layout.bottomMargin: 8
                spacing: 12

                Rectangle {
                    width: 32
                    height: 32
                    radius: 6
                    color: backMouse.containsMouse ? Theme.surfaceHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "\u2190"
                        color: Theme.textPrimary
                        font.pixelSize: 18
                    }

                    MouseArea {
                        id: backMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: root.back()
                    }
                }

                Text {
                    text: root.emuInfo.name
                    color: Theme.textPrimary
                    font.pixelSize: 24
                    font.weight: Font.Bold
                }

                Item { Layout.fillWidth: true }
            }

            // Two-column content
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                spacing: 32

                // LEFT COLUMN — info
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: 20

                    // Logo
                    Rectangle {
                        Layout.preferredWidth: 160
                        Layout.preferredHeight: 160
                        radius: 12
                        color: Theme.surface
                        clip: true

                        Image {
                            id: logoImg
                            anchors.centerIn: parent
                            width: parent.width - 16
                            height: parent.height - 16
                            source: logoForEmu(root.emuId)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            visible: false
                        }

                        Rectangle {
                            id: logoMask
                            anchors.fill: logoImg
                            radius: 10
                            visible: false
                        }

                        OpacityMask {
                            anchors.fill: logoImg
                            source: logoImg
                            maskSource: logoMask
                        }
                    }

                    // Version (installed only)
                    Text {
                        visible: root.emuInfo.installed && root.emuInfo.version !== ""
                        text: {
                            void(root._v)
                            return "Version: " + (root.emuInfo.version || "")
                        }
                        color: Theme.textDim
                        font.pixelSize: 12
                    }

                    // Description
                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true

                        Text {
                            text: "Description"
                            color: Theme.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }

                        Text {
                            text: root.emuInfo.description || ""
                            color: Theme.textDim
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    // Emulated Systems
                    ColumnLayout {
                        spacing: 4
                        Layout.fillWidth: true

                        Text {
                            text: "Emulated Systems"
                            color: Theme.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }

                        Text {
                            text: root.emuInfo.systems
                            color: Theme.textDim
                            font.pixelSize: 13
                        }
                    }

                    // BIOS Requirements
                    ColumnLayout {
                        spacing: 8
                        Layout.fillWidth: true

                        Text {
                            text: "BIOS"
                            color: Theme.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }

                        Text {
                            visible: !root.emuInfo.biosRequired
                            text: "No additional BIOS required."
                            color: Theme.textDim
                            font.pixelSize: 13
                        }

                        Rectangle {
                            visible: root.emuInfo.biosRequired && root.emuInfo.biosDetected
                            Layout.fillWidth: true
                            height: 36
                            radius: 8
                            color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.15)

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    void(root._v)
                                    return root.emuInfo.name + " BIOS detected!"
                                }
                                color: Theme.success
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                        }

                        ColumnLayout {
                            visible: root.emuInfo.biosRequired && !root.emuInfo.biosDetected
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                height: 36
                                radius: 8
                                color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.15)

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        void(root._v)
                                        return "BIOS required but not found."
                                    }
                                    color: Theme.error
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }
                            }

                            Button {
                                id: biosFolderBtn
                                implicitWidth: 160
                                implicitHeight: 32

                                background: Rectangle {
                                    radius: 6
                                    color: biosFolderBtn.hovered ? Theme.accentLight : Theme.accent
                                }

                                contentItem: Text {
                                    text: "Open BIOS Folder"
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: app.openBiosFolder()
                            }
                        }
                    }
                }

                // RIGHT COLUMN — actions (varies by install state)
                ColumnLayout {
                    Layout.preferredWidth: 280
                    Layout.alignment: Qt.AlignTop
                    spacing: 8

                    // ── UNINSTALLED STATE ──
                    ColumnLayout {
                        visible: !root.emuInfo.installed
                        spacing: 8

                        Text {
                            text: "Get Started"
                            color: Theme.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            Layout.bottomMargin: 4
                        }

                        Button {
                            id: installBtn
                            Layout.fillWidth: true
                            implicitHeight: 48

                            background: Rectangle {
                                radius: 8
                                color: installBtn.hovered ? Theme.accentLight : Theme.accent
                            }

                            contentItem: Text {
                                text: "Install " + root.emuInfo.name
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                progressPopup.title = "Installing " + root.emuInfo.name
                                progressPopup.subtitle = "Fetching release info..."
                                progressPopup.progressValue = -1
                                progressPopup.progressText = ""
                                progressPopup.accentColor = Theme.accent
                                progressPopup.open()
                                app.installEmulator(root.emuId)
                            }
                        }

                        Text {
                            text: "Downloads the latest release from GitHub"
                            color: Theme.textDim
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // ── INSTALLED STATE ──
                    ColumnLayout {
                        visible: root.emuInfo.installed
                        spacing: 8

                        Text {
                            text: "Actions"
                            color: Theme.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            Layout.bottomMargin: 4
                        }

                        ActionButton {
                            label: "Emulator Settings"
                            btnColor: Theme.accent
                            hoverColor: Theme.accentLight
                            onClicked: app.showEmulatorSettings(root.emuId)
                        }

                        ActionButton {
                            label: "Reset Configuration"
                            btnColor: Theme.accent
                            hoverColor: Theme.accentLight
                            onClicked: resetDialog.open()
                        }

                        ActionButton {
                            label: "Reinstall / Update"
                            btnColor: "#cc8800"
                            hoverColor: "#dd9900"
                            onClicked: {
                                progressPopup.title = "Reinstalling " + root.emuInfo.name
                                progressPopup.subtitle = "Fetching release info..."
                                progressPopup.progressValue = -1
                                progressPopup.progressText = ""
                                progressPopup.accentColor = Theme.accent
                                progressPopup.open()
                                app.installEmulator(root.emuId)
                            }
                        }

                        ActionButton {
                            label: "Uninstall"
                            btnColor: Theme.error
                            hoverColor: "#cc3333"
                            onClicked: uninstallDialog.open()
                        }

                        // Controls header
                        Text {
                            text: "Controls"
                            color: Theme.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            Layout.topMargin: 16
                            Layout.bottomMargin: 4
                        }

                        ActionButton {
                            label: "Controller Mapping"
                            btnColor: Theme.accent
                            hoverColor: Theme.accentLight
                            onClicked: app.showControllerMapping(root.emuId)
                        }

                        ActionButton {
                            label: "Hotkeys"
                            btnColor: Theme.accent
                            hoverColor: Theme.accentLight
                            onClicked: app.showHotkeySettings()
                        }
                    }
                }
            }

            Item { height: 32 }
        }
    }

    // ── Reusable action button ────────────────────────────
    component ActionButton: Button {
        id: actionBtn
        property string label: ""
        property color btnColor: Theme.accent
        property color hoverColor: Theme.accentLight

        Layout.fillWidth: true
        implicitHeight: 40

        background: Rectangle {
            radius: 8
            color: actionBtn.hovered ? actionBtn.hoverColor : actionBtn.btnColor
        }

        contentItem: Text {
            text: actionBtn.label
            color: Theme.textPrimary
            font.pixelSize: 13
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
```

Key changes from original:
- Added `version` to the default emuInfo fallback
- Added version display text below logo
- Right column now has two visibility-toggled sections: uninstalled (Install button) and installed (all actions)
- Reset Configuration button opens `resetDialog` instead of directly calling `resetConfiguration`
- Reinstall/Update opens progress popup before calling `installEmulator`
- Uninstall confirmation now opens progress popup and calls `uninstallEmulator` (which is now async)
- Added `ProgressPopup` instance and `Connections` for progress/finished signals
- Added reset confirmation dialog

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/EmulatorDetailPage.qml
git commit -m "feat: two-state detail page with version info, reset confirmation, and progress popups"
```

---

### Task 6: Enhanced resetConfiguration in AppController

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Update resetConfiguration to also reset controller mappings and hotkeys**

In `cpp/src/ui/app_controller.cpp`, replace the `resetConfiguration` method:

```cpp
void AppController::resetConfiguration(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const auto* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return;

    // Delete the existing config file
    QString configPath = adapter->configFilePath();
    if (!configPath.isEmpty() && QFileInfo::exists(configPath)) {
        QFile::remove(configPath);
        qInfo() << "[Reset] Removed config:" << configPath;
    }

    // Re-run ensureConfig to regenerate the fresh install defaults
    QString systemId = Paths::systemIdFor(emuId, manifest->systems);
    QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    QString savesPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath();
    adapter->ensureConfig(*manifest, biosPath, savesPath);

    // Also reset controller bindings and settings to defaults
    resetControllerBindings(emuId);
    resetControllerSettings(emuId);

    // Reset hotkeys to defaults (unified across all emulators)
    resetHotkeys();

    setStatus(manifest->name + " configuration reset to install defaults.");
    emit configurationReset(emuId);
}
```

The only change is adding the three reset calls after `ensureConfig`. Note: `resetControllerBindings`, `resetControllerSettings`, and `resetHotkeys` already exist and write defaults to the INI. Since we just recreated the config file via `ensureConfig`, these calls will write the default bindings/settings/hotkeys into the fresh config.

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

- [ ] **Step 3: Commit**

```bash
git add cpp/src/ui/app_controller.cpp
git commit -m "feat: resetConfiguration now resets controller mappings and hotkeys too"
```

---

### Task 7: Update Check Service

**Files:**
- Modify: `cpp/src/services/emulator_service.h`
- Modify: `cpp/src/services/emulator_service.cpp`
- Modify: `cpp/src/ui/app_controller.h`
- Modify: `cpp/src/ui/app_controller.cpp`

- [ ] **Step 1: Add update check method and signal to EmulatorService**

In `cpp/src/services/emulator_service.h`, add after the `saveVersion` method:

```cpp
    /** Check GitHub for updates to installed emulators (async, rate-limited to once/day). */
    void checkForUpdates();
```

Add a new signal:

```cpp
    void updateAvailable(const QString& emuId, const QString& currentVersion,
                         const QString& latestVersion);
```

- [ ] **Step 2: Implement checkForUpdates in EmulatorService**

In `cpp/src/services/emulator_service.cpp`, add at the bottom:

```cpp
// ── Update checking ───────────────────────────────────────

void EmulatorService::checkForUpdates() {
    // Rate limit: only check once per day
    QString cacheFile = Paths::rootDir() + "/update_check.json";
    QFile cache(cacheFile);
    if (cache.open(QIODevice::ReadOnly)) {
        QJsonDocument cacheDoc = QJsonDocument::fromJson(cache.readAll());
        cache.close();
        QString lastCheck = cacheDoc.object()["last_check"].toString();
        QDateTime lastTime = QDateTime::fromString(lastCheck, Qt::ISODate);
        if (lastTime.isValid() && lastTime.secsTo(QDateTime::currentDateTimeUtc()) < 86400) {
            // Use cached results
            QJsonObject updates = cacheDoc.object()["updates"].toObject();
            for (auto it = updates.begin(); it != updates.end(); ++it) {
                QJsonObject u = it.value().toObject();
                emit updateAvailable(it.key(), u["current"].toString(), u["latest"].toString());
            }
            return;
        }
    }

    // Check each installed emulator on a background thread
    // Collect manifest data on main thread first
    struct CheckItem {
        QString emuId;
        QString githubRepo;
        QString currentVersion;
        QString installFolder;
    };

    QVector<CheckItem> items;
    for (const auto& emu : m_loader->allEmulators()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emu.id);
        if (!adapter || !adapter->isInstalled(emu)) continue;

        QString version = installedVersion(emu.id);
        if (version.isEmpty()) continue;

        items.append({emu.id, emu.github_repo, version, emu.install_folder});
    }

    if (items.isEmpty()) return;

    QtConcurrent::run([this, items, cacheFile]() {
        QJsonObject updates;

        for (const auto& item : items) {
            QString apiUrl = "https://api.github.com/repos/" + item.githubRepo + "/releases/latest";

            // Synchronous HTTP on background thread
            QNetworkAccessManager nam;
            QNetworkRequest req{QUrl(apiUrl)};
            req.setHeader(QNetworkRequest::UserAgentHeader, "EmulatorFrontend/1.0");
            req.setRawHeader("Accept", "application/json");

            QNetworkReply* reply = nam.get(req);
            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            if (reply->error() != QNetworkReply::NoError) {
                reply->deleteLater();
                continue;
            }

            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            reply->deleteLater();

            QString latestTag = doc.object()["tag_name"].toString();
            if (latestTag.isEmpty() || latestTag == item.currentVersion) continue;

            QJsonObject u;
            u["current"] = item.currentVersion;
            u["latest"] = latestTag;
            updates[item.emuId] = u;

            // Emit on main thread via QMetaObject::invokeMethod
            QMetaObject::invokeMethod(this, [this, emuId = item.emuId,
                                              current = item.currentVersion, latestTag]() {
                emit updateAvailable(emuId, current, latestTag);
            }, Qt::QueuedConnection);
        }

        // Save cache
        QJsonObject cacheObj;
        cacheObj["last_check"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        cacheObj["updates"] = updates;

        QMetaObject::invokeMethod(this, [cacheFile, cacheObj]() {
            QFile file(cacheFile);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(cacheObj).toJson(QJsonDocument::Compact));
            }
        }, Qt::QueuedConnection);
    });
}
```

Add the missing include at the top of the file:

```cpp
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
```

- [ ] **Step 3: Add updateAvailable signal and checkForUpdates call to AppController**

In `cpp/src/ui/app_controller.h`, add a new signal:

```cpp
    void updateAvailable(const QString& emuId, const QString& currentVersion,
                         const QString& latestVersion);
```

Add a new public method:

```cpp
    Q_INVOKABLE void checkForUpdates();
```

In `cpp/src/ui/app_controller.cpp`, add the method implementation:

```cpp
void AppController::checkForUpdates() {
    m_emuService.checkForUpdates();
}
```

In the constructor, add after the existing service connections:

```cpp
    connect(&m_emuService, &EmulatorService::updateAvailable,
            this, &AppController::updateAvailable);
```

- [ ] **Step 4: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/emulator_service.h cpp/src/services/emulator_service.cpp \
        cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat: add startup update check with rate-limited GitHub API polling"
```

---

### Task 8: UpdateNotification QML Component

**Files:**
- Create: `cpp/qml/AppUI/UpdateNotification.qml`
- Modify: `cpp/qml/AppUI/AppWindow.qml`
- Modify: `cpp/qml/AppUI/SettingsOverlay.qml`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Create UpdateNotification.qml**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: notification
    width: notifRow.width + 32
    height: 48
    radius: 10
    color: Theme.surface
    border.width: 1
    border.color: Theme.accent
    opacity: 0.0
    visible: opacity > 0
    z: 200

    property string emuId: ""
    property string emuName: ""
    property string currentVersion: ""
    property string latestVersion: ""

    // Queue of pending updates
    property var updateQueue: []

    function showUpdate(emuId, currentVersion, latestVersion) {
        // Look up emulator name from allEmulatorStatus
        var emulators = app.allEmulatorStatus()
        var name = emuId
        for (var i = 0; i < emulators.length; i++) {
            if (emulators[i].id === emuId) {
                name = emulators[i].name
                break
            }
        }

        if (notification.opacity > 0) {
            // Queue it
            updateQueue.push({ emuId: emuId, name: name,
                              current: currentVersion, latest: latestVersion })
            return
        }

        notification.emuId = emuId
        notification.emuName = name
        notification.currentVersion = currentVersion
        notification.latestVersion = latestVersion
        showAnim.start()
        autoDismissTimer.restart()
    }

    function dismiss() {
        hideAnim.start()
    }

    function showNext() {
        if (updateQueue.length > 0) {
            var next = updateQueue.shift()
            notification.emuId = next.emuId
            notification.emuName = next.name
            notification.currentVersion = next.current
            notification.latestVersion = next.latest
            showAnim.start()
            autoDismissTimer.restart()
        }
    }

    NumberAnimation on opacity {
        id: showAnim
        from: 0.0; to: 1.0; duration: 300
        running: false
    }

    NumberAnimation on opacity {
        id: hideAnim
        from: 1.0; to: 0.0; duration: 300
        running: false
        onFinished: notification.showNext()
    }

    Timer {
        id: autoDismissTimer
        interval: 10000
        onTriggered: notification.dismiss()
    }

    RowLayout {
        id: notifRow
        anchors.centerIn: parent
        spacing: 12

        Text {
            text: notification.emuName + " " + notification.latestVersion +
                  " available (you have " + notification.currentVersion + ")"
            color: Theme.textPrimary
            font.pixelSize: 13
        }

        // View button
        Rectangle {
            width: 60
            height: 28
            radius: 6
            color: viewMa.containsMouse ? Theme.accentLight : Theme.accent

            Text {
                anchors.centerIn: parent
                text: "View"
                color: Theme.textPrimary
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            MouseArea {
                id: viewMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    notification.dismiss()
                    // Navigate to settings overlay > emulators > detail page
                    settingsOverlay.navigateToEmulator(notification.emuId)
                }
            }
        }

        // Close button
        Rectangle {
            width: 24
            height: 24
            radius: 4
            color: closeMa.containsMouse ? Theme.surfaceHover : "transparent"

            Text {
                anchors.centerIn: parent
                text: "\u2715"
                color: Theme.textDim
                font.pixelSize: 12
            }

            MouseArea {
                id: closeMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: notification.dismiss()
            }
        }
    }
}
```

- [ ] **Step 2: Add navigateToEmulator to SettingsOverlay**

In `cpp/qml/AppUI/SettingsOverlay.qml`, add a new function after the `close()` function:

```qml
    function navigateToEmulator(emuId) {
        visible = true
        selectedCategory = 0  // Emulators page
        // The EmulatorManagePage will need to select this emulator
        // We'll use a property on the overlay that EmulatorManagePage watches
        targetEmuId = emuId
    }

    property string targetEmuId: ""
```

- [ ] **Step 3: Update EmulatorManagePage to watch for targetEmuId**

In `cpp/qml/AppUI/EmulatorManagePage.qml`, update to watch the overlay's targetEmuId. Replace the file:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string selectedEmuId: ""

    // Watch for external navigation requests (from update notification)
    Connections {
        target: settingsOverlay
        function onTargetEmuIdChanged() {
            if (settingsOverlay.targetEmuId !== "") {
                root.selectedEmuId = settingsOverlay.targetEmuId
                settingsOverlay.targetEmuId = ""
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: selectedEmuId === "" ? 0 : 1

        EmulatorManageGrid {
            onEmulatorSelected: function(emuId) {
                root.selectedEmuId = emuId
            }
        }

        EmulatorDetailPage {
            emuId: root.selectedEmuId
            onBack: root.selectedEmuId = ""
        }
    }
}
```

- [ ] **Step 4: Add UpdateNotification to AppWindow.qml**

In `cpp/qml/AppUI/AppWindow.qml`, add the UpdateNotification component. Find the `StatusBar` block and add the notification just before it:

```qml
    // Update notification toast
    UpdateNotification {
        id: updateNotification
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
    }

    Connections {
        target: app
        function onUpdateAvailable(emuId, currentVersion, latestVersion) {
            updateNotification.showUpdate(emuId, currentVersion, latestVersion)
        }
    }
```

Also add the startup check call. Find the `Component.onCompleted` block (if one exists in AppWindow or the main StackView), and add:

```qml
    Component.onCompleted: {
        app.checkForUpdates()
    }
```

If there's already a `Component.onCompleted`, just add `app.checkForUpdates()` inside the existing one.

- [ ] **Step 5: Register UpdateNotification.qml in CMakeLists.txt**

In `cpp/CMakeLists.txt`, find the `qt_add_qml_module` block for `appui_module` and add `UpdateNotification.qml` to the QML_FILES list.

- [ ] **Step 6: Build and verify**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

- [ ] **Step 7: Commit**

```bash
git add cpp/qml/AppUI/UpdateNotification.qml cpp/qml/AppUI/AppWindow.qml \
        cpp/qml/AppUI/SettingsOverlay.qml cpp/qml/AppUI/EmulatorManagePage.qml \
        cpp/CMakeLists.txt
git commit -m "feat: startup update notification with navigation to emulator detail page"
```

---

### Task 9: Settings Audit — Verify PCSX2 and DuckStation Settings

**Files:**
- Possibly modify: `cpp/src/adapters/pcsx2_adapter.cpp`
- Possibly modify: `cpp/src/adapters/duckstation_adapter.cpp`

This task is an audit rather than a feature. The goal is to verify that all settings schemas work correctly with the existing `EmulatorSettingsPage` dialog.

- [ ] **Step 1: Install both emulators and verify INI files exist**

Run the app and install both emulators (or verify they're already installed):

```bash
cd cpp && ./build/EmulatorFrontend --cli --install pcsx2
./build/EmulatorFrontend --cli --install duckstation
```

Check that config files were created:

```bash
ls -la ~/EmulatorFrontend/emulators/pcsx2/inis/PCSX2.ini
ls -la ~/EmulatorFrontend/emulators/duckstation/settings.ini
```

(Paths may vary based on `Paths::rootDir()` — check what `Paths::emulatorsDir()` returns.)

- [ ] **Step 2: Cross-reference PCSX2 settingsSchema keys against actual INI**

For each setting in `pcsx2_adapter.cpp` `settingsSchema()`, verify the section/key exists in the generated `PCSX2.ini`. Focus on:
- Are any schema keys missing from the INI? (These should be greyed out per CLAUDE.md rules)
- Are the `options` values correct for combo boxes?
- Do `minVal`/`maxVal` ranges make sense?

Check by reading the INI and comparing sections:

```bash
grep -c "^\[" ~/EmulatorFrontend/emulators/pcsx2/inis/PCSX2.ini  # count sections
```

Log any findings and fix incorrect section/key names in the adapter.

- [ ] **Step 3: Cross-reference DuckStation settingsSchema keys against actual INI**

Same audit for DuckStation. Check `settings.ini` against `duckstation_adapter.cpp` `settingsSchema()`.

```bash
grep -c "^\[" ~/EmulatorFrontend/emulators/duckstation/settings.ini
```

- [ ] **Step 4: Launch the app, open Emulator Settings for each emulator, and verify**

Run:
```bash
cd cpp && ./build/EmulatorFrontend
```

For each emulator:
1. Settings > Emulators > click emulator > Emulator Settings
2. Verify all categories load
3. Verify combo boxes show correct options
4. Change a setting, close, reopen — verify it persisted
5. Verify stale keys are greyed out with tooltip

- [ ] **Step 5: Fix any issues found and commit**

If any schema keys need updating:

```bash
git add cpp/src/adapters/pcsx2_adapter.cpp cpp/src/adapters/duckstation_adapter.cpp
git commit -m "fix: correct settings schema keys for PCSX2 and DuckStation adapters"
```

If no issues found, skip this commit.

---

### Task 10: Final Integration Test

**Files:** None (testing only)

- [ ] **Step 1: Build clean**

```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build 2>&1 | tail -20
```

- [ ] **Step 2: Test the full emulator lifecycle**

Launch the app:
```bash
cd cpp && ./build/EmulatorFrontend
```

Test sequence:
1. Open Settings (Escape) > Emulators
2. Verify grid shows both installed and uninstalled emulators (uninstalled greyed out with badge)
3. Click an uninstalled emulator — verify detail page shows Install button
4. Click Install — verify progress popup with real download progress
5. After install completes — verify page transitions to installed state with version shown
6. Click Emulator Settings — verify settings dialog opens and works
7. Click Reset Configuration — verify confirmation dialog appears
8. Confirm reset — verify settings, controller mappings, and hotkeys reset
9. Click Reinstall / Update — verify progress popup
10. Click Uninstall — verify confirmation dialog, then progress popup, then returns to grid
11. Close and reopen app — verify update notification appears (if update available)
12. Click "View" on notification — verify navigates to emulator detail page

- [ ] **Step 3: Test CLI mode still works**

```bash
cd cpp && ./build/EmulatorFrontend --cli --install pcsx2
```

Verify it installs successfully with the synchronous path.

- [ ] **Step 4: Commit any final fixes**

If any issues were found and fixed during testing:

```bash
git add -A
git commit -m "fix: integration test fixes for emulator management redesign"
```
