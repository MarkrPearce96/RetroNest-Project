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
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>

#include "adapters/adapter_registry.h"

// ============================================================================
// HTTP helpers (synchronous via local event loop)
// ============================================================================

static QByteArray httpGet(const QString& url) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(30000);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Installer] HTTP error:" << reply->errorString();
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

// ============================================================================
// Synchronous file download (used by installSync / CLI mode)
// ============================================================================

static bool downloadSync(const QString& url, const QString& destPath) {
    qInfo() << "[Installer] Downloading" << url;

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(120000);  // 2 min for large downloads
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
// Extract archive — isolated so extraction method can be swapped later
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
        // Mount, copy .app, unmount
        QTemporaryDir mountPoint;
        if (!mountPoint.isValid()) return false;
        mountPoint.setAutoRemove(false);  // prevent deletion while detach runs

        QProcess mount;
        mount.start("hdiutil", {"attach", absArchive, "-mountpoint",
                                mountPoint.path(), "-nobrowse", "-quiet"});
        mount.waitForFinished(60000);
        if (mount.exitCode() != 0) {
            qWarning() << "[Installer] hdiutil attach failed:" << mount.readAllStandardError();
            QDir(mountPoint.path()).removeRecursively();
            return false;
        }

        // Find .app inside mount
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
        detach.waitForFinished(30000);  // wait for detach to complete before cleanup
        QDir(mountPoint.path()).removeRecursively();
        return ok;
    } else if (name.endsWith(".appimage")) {
        // AppImage: just copy and make executable
        const QString dest = absDest + "/" + QFileInfo(absArchive).fileName();
        if (!QFile::copy(archivePath, dest)) {
            qWarning() << "[Installer] Failed to copy AppImage to" << dest;
            return false;
        }
        if (QProcess::execute("chmod", {"755", dest}) != 0) {
            qWarning() << "[Installer] Failed to set AppImage executable:" << dest;
            return false;
        }
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
// Asset matching — pick the right download for this platform
// ============================================================================

QString EmulatorInstaller::matchAsset(const QString& emuId, const QStringList& assetNames) {
    // Delegate to the adapter if one is registered
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (adapter)
        return adapter->matchAsset(assetNames);

    // Fallback: generic platform-keyword matching (no adapter registered)
#if defined(Q_OS_MACOS)
    const QString platform = "mac";
#elif defined(Q_OS_WIN)
    const QString platform = "windows";
#else
    const QString platform = "linux";
#endif

    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
        if (lower.contains(platform) &&
            (name.endsWith(".zip") || name.endsWith(".tar.xz") ||
             name.endsWith(".dmg") || name.endsWith(".tar.gz"))) {
            return name;
        }
    }

    return {};
}

// ============================================================================
// Helper: parse release JSON and resolve asset URL
// ============================================================================

EmulatorInstaller::ReleaseInfo EmulatorInstaller::fetchReleaseInfo(const EmulatorManifest& manifest) {
    ReleaseInfo info;

    const QString apiUrl = "https://api.github.com/repos/" + manifest.github_repo + "/releases/latest";
    QByteArray releaseJson = httpGet(apiUrl);
    if (releaseJson.isEmpty()) {
        info.errorMessage = "Failed to fetch release info from GitHub";
        return info;
    }

    QJsonDocument doc = QJsonDocument::fromJson(releaseJson);
    if (!doc.isObject()) {
        info.errorMessage = "Invalid release JSON";
        return info;
    }

    QJsonObject release = doc.object();
    info.tagName = release["tag_name"].toString();
    info.publishedAt = release["published_at"].toString();
    QJsonArray assets = release["assets"].toArray();

    qInfo() << "[Installer] Latest release:" << info.tagName << "with" << assets.size() << "assets";

    QStringList assetNames;
    QHash<QString, QString> assetUrls;
    for (const auto& a : assets) {
        QJsonObject asset = a.toObject();
        QString name = asset["name"].toString();
        QString url = asset["browser_download_url"].toString();
        assetNames.append(name);
        assetUrls.insert(name, url);
    }

    info.assetName = matchAsset(manifest.id, assetNames);
    if (info.assetName.isEmpty()) {
        info.errorMessage = "No matching asset for platform. Available: " + assetNames.join(", ");
        return info;
    }

    info.downloadUrl = assetUrls[info.assetName];
    info.ok = true;
    qInfo() << "[Installer] Selected asset:" << info.assetName;
    return info;
}

// ============================================================================
// Post-download: extract, cleanup, quarantine removal
// ============================================================================

EmulatorInstaller::InstallResult EmulatorInstaller::postDownload(
    const QString& tempFile, const QString& installPath, const QString& tagName,
    const QString& publishedAt)
{
    InstallResult result;
    result.version = tagName;
    result.publishedAt = publishedAt;

    if (!extract(tempFile, installPath)) {
        QFile::remove(tempFile);
        result.message = "Extraction failed";
        return result;
    }

    QFile::remove(tempFile);

#if defined(Q_OS_MACOS)
    QProcess::execute("xattr", {"-rd", "com.apple.quarantine", installPath});
#endif

    result.success = true;
    result.message = "Successfully installed";
    return result;
}

// ============================================================================
// Constructor
// ============================================================================

EmulatorInstaller::EmulatorInstaller(QObject* parent)
    : QObject(parent) {}

// ============================================================================
// Synchronous install (CLI mode)
// ============================================================================

EmulatorInstaller::InstallResult EmulatorInstaller::installSync(
    const EmulatorManifest& manifest, const QString& installPath)
{
    qInfo() << "[Installer] Installing" << manifest.name << "from" << manifest.github_repo;

    InstallResult result;

    // 1. Fetch release metadata
    ReleaseInfo release = fetchReleaseInfo(manifest);
    if (!release.ok) {
        qWarning() << "[Installer]" << release.errorMessage;
        result.message = release.errorMessage;
        return result;
    }

    result.version = release.tagName;

    // 2. Download
    QDir().mkpath(installPath);
    const QString tempFile = installPath + "/" + release.assetName;

    if (!downloadSync(release.downloadUrl, tempFile)) {
        result.message = "Download failed";
        return result;
    }

    // 3. Extract, cleanup, quarantine
    result = postDownload(tempFile, installPath, release.tagName, release.publishedAt);

    if (result.success) {
        qInfo() << "[Installer] Successfully installed" << manifest.name
                << "(" << result.version << ") to" << installPath;
    }

    return result;
}

// ============================================================================
// Async install (GUI mode) — progress signals for download + extraction
// ============================================================================

void EmulatorInstaller::installAsync(const EmulatorManifest& manifest, const QString& installPath) {
    qInfo() << "[Installer] Async installing" << manifest.name << "from" << manifest.github_repo;

    // Phase 1: Fetch release metadata asynchronously
    emit progress(0.0, "Fetching", "Fetching release info...");

    const QString apiUrl = "https://api.github.com/repos/" + manifest.github_repo + "/releases/latest";
    const QString emuId = manifest.id;

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest apiReq{QUrl(apiUrl)};
    apiReq.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    apiReq.setRawHeader("Accept", "application/json");

    QNetworkReply* apiReply = nam->get(apiReq);

    connect(apiReply, &QNetworkReply::finished, this,
            [this, apiReply, nam, emuId, installPath]() {
                apiReply->deleteLater();

                if (apiReply->error() != QNetworkReply::NoError) {
                    qWarning() << "[Installer] API fetch failed:" << apiReply->errorString();
                    nam->deleteLater();
                    emit finished(InstallResult{false, "Failed to fetch release info: " + apiReply->errorString(), {}});
                    return;
                }

                QByteArray releaseJson = apiReply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(releaseJson);
                if (!doc.isObject()) {
                    nam->deleteLater();
                    emit finished(InstallResult{false, "Invalid release JSON", {}});
                    return;
                }

                QJsonObject release = doc.object();
                QString tagName = release["tag_name"].toString();
                QString publishedAt = release["published_at"].toString();
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

                QString assetName = matchAsset(emuId, assetNames);
                if (assetName.isEmpty()) {
                    nam->deleteLater();
                    QString msg = "No matching asset for platform. Available: " + assetNames.join(", ");
                    qWarning() << "[Installer]" << msg;
                    emit finished(InstallResult{false, msg, tagName, publishedAt});
                    return;
                }

                QString downloadUrl = assetUrls[assetName];
                qInfo() << "[Installer] Selected asset:" << assetName;

                // Phase 2: Download with byte-level progress and incremental writes
                QDir().mkpath(installPath);
                const QString tempFile = installPath + "/" + assetName;

                auto* file = new QFile(tempFile, this);
                if (!file->open(QIODevice::WriteOnly)) {
                    qWarning() << "[Installer] Cannot write to" << tempFile;
                    nam->deleteLater();
                    file->deleteLater();
                    emit finished(InstallResult{false, "Cannot write temp file", tagName, publishedAt});
                    return;
                }

                QNetworkRequest dlReq{QUrl(downloadUrl)};
                dlReq.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
                dlReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                   QNetworkRequest::NoLessSafeRedirectPolicy);

                QNetworkReply* dlReply = nam->get(dlReq);

                // Write data incrementally as it arrives
                connect(dlReply, &QNetworkReply::readyRead, this,
                        [file, dlReply]() {
                            file->write(dlReply->readAll());
                        });

                connect(dlReply, &QNetworkReply::downloadProgress, this,
                        [this](qint64 received, qint64 total) {
                            if (total <= 0) {
                                emit progress(-1.0, "Downloading", "Downloading...");
                                return;
                            }
                            double ratio = static_cast<double>(received) / static_cast<double>(total);
                            int percent = static_cast<int>(ratio * 100.0);
                            qint64 receivedMB = received / (1024 * 1024);
                            qint64 totalMB = total / (1024 * 1024);
                            QString detail = QString("%1% \u2014 %2 MB / %3 MB")
                                                 .arg(percent).arg(receivedMB).arg(totalMB);
                            emit progress(ratio, "Downloading", detail);
                        });

                connect(dlReply, &QNetworkReply::finished, this,
                        [this, dlReply, nam, file, tempFile, installPath, tagName, publishedAt]() {
                            dlReply->deleteLater();
                            nam->deleteLater();
                            file->close();
                            file->deleteLater();

                            if (dlReply->error() != QNetworkReply::NoError) {
                                qWarning() << "[Installer] Download failed:" << dlReply->errorString();
                                QFile::remove(tempFile);
                                emit finished(InstallResult{false, "Download failed: " + dlReply->errorString(), tagName, publishedAt});
                                return;
                            }

                            qInfo() << "[Installer] Downloaded to" << tempFile
                                    << "(" << QFileInfo(tempFile).size() / (1024 * 1024) << "MB)";

                            // Phase 3: Extract on background thread
                            emit progress(-1.0, "Extracting", "Extracting files...");

                            auto* watcher = new QFutureWatcher<InstallResult>(this);
                            connect(watcher, &QFutureWatcher<InstallResult>::finished, this,
                                    [this, watcher]() {
                                        InstallResult result = watcher->result();
                                        watcher->deleteLater();

                                        if (result.success) {
                                            qInfo() << "[Installer] Async install complete ("
                                                    << result.version << ")";
                                        }
                                        emit finished(result);
                                    });

                            QFuture<InstallResult> future = QtConcurrent::run(
                                [tempFile, installPath, tagName, publishedAt]() -> InstallResult {
                                    return postDownload(tempFile, installPath, tagName, publishedAt);
                                });

                            watcher->setFuture(future);
                        });
            });
}
