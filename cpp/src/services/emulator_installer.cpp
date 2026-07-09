#include "emulator_installer.h"
#include "installer_utils.h"

#include <QCoreApplication>
#include <QDateTime>
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
#include "github_credentials.h"

// ============================================================================
// Synchronous file download (used by installSync / CLI mode)
// ============================================================================

static bool downloadSync(const QString& url, const QString& destPath,
                         const QString& authToken = {}) {
    qInfo() << "[Installer] Downloading" << url;

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    if (!authToken.isEmpty()) {
        req.setRawHeader("Accept", "application/octet-stream");
        req.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());
    }
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
    } else {
        // Process-era retirement (2026-07): .dmg / .AppImage app-bundle
        // handling deleted — every emulator installs as a core zip
        // (deploy-contract.md); tar kept as generic archive plumbing.
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
             name.endsWith(".tar.gz"))) {
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

    // Some emulators (e.g. Dolphin) distribute outside GitHub Releases.
    // Consult the adapter first; if it returns a direct download, skip the
    // /releases/latest API entirely.
    if (auto* adapter = AdapterRegistry::instance().adapterFor(manifest.id)) {
        const auto direct = adapter->resolveDirectDownload(manifest);
        if (!direct.downloadUrl.isEmpty()) {
            info.tagName = direct.version;
            info.publishedAt = direct.publishedAt;
            info.assetName = direct.assetName;
            info.downloadUrl = direct.downloadUrl;
            info.sha256 = direct.sha256;
            info.ok = true;
            qInfo() << "[Installer] Using adapter-resolved direct download for" << manifest.id
                    << ":" << direct.assetName << "(" << direct.version << ")";
            return info;
        }
    }

    const QString apiUrl = "https://api.github.com/repos/" + manifest.github_repo + "/releases/latest";
    const QString authToken =
        manifest.is_private ? GitHubCredentials::token() : QString();
    if (manifest.is_private && authToken.isEmpty()) {
        info.errorMessage = manifest.name +
            " requires a GitHub token (private build) — not available in this build.";
        return info;
    }
    QByteArray releaseJson = InstallerUtils::httpGet(apiUrl, 30000, "[Installer]", authToken);
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
    QHash<QString, QString> assetDigests;  // GitHub Releases assets carry an
                                            // optional "digest" field formatted as
                                            // "sha256:<hex>". Empty if upstream
                                            // didn't compute it.
    for (const auto& a : assets) {
        QJsonObject asset = a.toObject();
        QString name = asset["name"].toString();
        QString url = InstallerUtils::chooseAssetDownloadUrl(asset, manifest.is_private);
        QString digest = asset["digest"].toString();
        assetNames.append(name);
        assetUrls.insert(name, url);
        if (digest.startsWith("sha256:", Qt::CaseInsensitive))
            assetDigests.insert(name, digest.mid(7).toLower());
    }

    info.assetName = matchAsset(manifest.id, assetNames);
    if (info.assetName.isEmpty()) {
        info.errorMessage = "No matching asset for platform. Available: " + assetNames.join(", ");
        return info;
    }

    info.downloadUrl = assetUrls[info.assetName];
    info.sha256 = assetDigests.value(info.assetName);  // empty if not provided
    info.ok = true;
    qInfo() << "[Installer] Selected asset:" << info.assetName
            << (info.sha256.isEmpty() ? "(no digest)" : "(digest provided)");
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

    // Libretro cores arrive as a single .dylib.zip — unzip into cores/ and
    // strip quarantine on the dylib itself rather than the whole install dir.
    if (tempFile.endsWith(".dylib.zip", Qt::CaseInsensitive)) {
        const QString coreDir = installPath + "/cores";
        QDir().mkpath(coreDir);

        QProcess unzip;
        unzip.start("/usr/bin/unzip", {"-o", tempFile, "-d", coreDir});
        unzip.waitForFinished(30000);
        if (unzip.exitCode() != 0) {
            qWarning() << "[Installer] Libretro unzip failed:" << unzip.readAllStandardError();
            QFile::remove(tempFile);
            result.message = "Extraction failed";
            return result;
        }
        QFile::remove(tempFile);

        // Derive the dylib name by stripping .zip from the archive name.
        const QString dylibName = QFileInfo(tempFile).fileName().chopped(4); // strip ".zip"
        const QString dylibPath = coreDir + "/" + dylibName;

#if defined(Q_OS_MACOS)
        // The core, plus any dylibs shipped beside it, must all be
        // quarantine-free and ad-hoc signed before they can load in-process:
        //  - quarantine: a downloaded/unzipped file dlopen()s to a Gatekeeper
        //    denial otherwise.
        //  - signing: an unsigned dylib can't allocate JIT/executable memory
        //    under the hardened runtime, so PCSX2's recompilers fail and the
        //    VM dies during init with no output (observed 2026-07-04).
        // The sibling dirs are the release's bundled dependencies:
        //    <base>_libs      — non-system deps bundled by CI (dylibbundler),
        //                       loaded via @loader_path/<base>_libs/ so the
        //                       core doesn't depend on the user's Homebrew
        //                       versions (a mismatch = silent dlopen failure;
        //                       this was the actual 2026-07-04 pcsx2 break).
        //    <base>_resources — shader/metallib and data files.
        const QString baseName = dylibName.chopped(6);  // strip ".dylib"
        const QString libsSubdir = baseName + "_libs";
        QStringList dylibsToProcess{dylibPath};
        QStringList bundledLibs;  // just the <base>_libs dylibs (need path repair)
        for (const QString& suffix : {"_libs", "_resources"}) {
            const QString dir = coreDir + "/" + baseName + suffix;
            if (QFileInfo(dir).isDir()) {
                for (const QFileInfo& f : QDir(dir).entryInfoList({"*.dylib"}, QDir::Files)) {
                    dylibsToProcess << f.absoluteFilePath();
                    if (suffix == QLatin1String("_libs"))
                        bundledLibs << f.absoluteFilePath();
                }
            }
        }

        // Repair inter-library load paths in <base>_libs. dylibbundler applies
        // ONE install prefix (@loader_path/<base>_libs/) to every rewrite, but
        // for a lib that itself lives inside <base>_libs the loader dir IS
        // <base>_libs — so a sibling ref becomes @loader_path/<base>_libs/<base>_libs/lib
        // (doubled → not found → the whole core fails to dlopen). Rewrite each
        // bundled lib's sibling references down to @loader_path/<lib>. The main
        // core (which lives one level up in cores/) keeps the subdir prefix.
        // Done before signing so the fix is covered by the ad-hoc signature.
        const QString doubledPrefix = "@loader_path/" + libsSubdir + "/";
        for (const QString& lib : bundledLibs) {
            QProcess otool;
            otool.start("/usr/bin/otool", {"-L", lib});
            otool.waitForFinished(5000);
            const QList<QByteArray> lines = otool.readAllStandardOutput().split('\n');
            for (const QByteArray& raw : lines) {
                const QString dep = QString::fromUtf8(raw).trimmed().section(' ', 0, 0);
                if (!dep.startsWith(doubledPrefix)) continue;
                const QString fixed = "@loader_path/" + dep.mid(doubledPrefix.size());
                QProcess fix;
                fix.start("/usr/bin/install_name_tool", {"-change", dep, fixed, lib});
                fix.waitForFinished(5000);
            }
        }

        for (const QString& path : dylibsToProcess) {
            QProcess xattr;
            xattr.start("/usr/bin/xattr", {"-d", "com.apple.quarantine", path});
            xattr.waitForFinished(2000);  // absent attr → nonzero, ignored
            QProcess csign;
            csign.start("/usr/bin/codesign", {"--force", "--sign", "-", path});
            csign.waitForFinished(15000);
            if (csign.exitCode() != 0)
                qWarning() << "[Installer] codesign failed for" << path
                           << csign.readAllStandardError();
        }
        qInfo() << "[Installer] repaired" << bundledLibs.size() << "bundled libs, dequarantined + signed"
                << dylibsToProcess.size() << "dylib(s) for" << dylibName;
#endif

        // Core release zips ship a root-level VERSION file; unzipping into
        // the shared cores/ dir means each install clobbers the previous
        // core's copy. Keep it, but namespaced per core.
        const QString looseVersionFile = coreDir + "/VERSION";
        if (QFileInfo::exists(looseVersionFile)) {
            const QString renamed = coreDir + "/" + dylibName + ".VERSION.txt";
            QFile::remove(renamed);  // overwrite ok
            if (QFile::rename(looseVersionFile, renamed))
                qInfo() << "[Installer] Renamed loose VERSION file to" << renamed;
            else
                qWarning() << "[Installer] Failed to rename" << looseVersionFile
                           << "to" << renamed;
        }

        // Write the per-core version record (JSON, same shape as the legacy
        // shared .version.json). EmulatorService::readVersionRecord treats
        // this sidecar as the source of truth for libretro cores.
        QJsonObject record;
        record["version"] = tagName;
        record["published_at"] = publishedAt;
        record["installed_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        QFile vf(dylibPath + ".version");
        if (vf.open(QIODevice::WriteOnly))
            vf.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
        else
            qWarning() << "[Installer] Failed to write version sidecar:" << vf.fileName();

        qInfo() << "[Installer] Libretro core installed to" << dylibPath;
        result.success = true;
        result.message = "Successfully installed";
        return result;
    }

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

    const QString authToken =
        manifest.is_private ? GitHubCredentials::token() : QString();
    if (!downloadSync(release.downloadUrl, tempFile, authToken)) {
        result.message = "Download failed";
        return result;
    }

    // 2.5. Verify integrity (no-op if upstream didn't provide a digest).
    if (!InstallerUtils::verifySha256(tempFile, release.sha256, "[Installer]")) {
        QFile::remove(tempFile);
        result.message = "Integrity check failed (SHA256 mismatch).";
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

    // Adapter override: some emulators (e.g. Dolphin) distribute outside
    // GitHub Releases. If the adapter resolves a direct download, jump
    // straight to the download phase and reuse the existing async pipeline.
    if (auto* adapter = AdapterRegistry::instance().adapterFor(manifest.id)) {
        const auto direct = adapter->resolveDirectDownload(manifest);
        if (!direct.downloadUrl.isEmpty()) {
            qInfo() << "[Installer] Using adapter-resolved direct download for" << manifest.id
                    << ":" << direct.assetName << "(" << direct.version << ")";
            startDirectDownload(direct.assetName, direct.downloadUrl,
                                direct.version, direct.publishedAt,
                                direct.sha256, installPath);
            return;
        }
    }

    const QString apiUrl = "https://api.github.com/repos/" + manifest.github_repo + "/releases/latest";
    const QString emuId = manifest.id;
    const QString authToken =
        manifest.is_private ? GitHubCredentials::token() : QString();

    if (manifest.is_private && authToken.isEmpty()) {
        emit finished(InstallResult{false, manifest.name +
            " requires a GitHub token (private build) — not available in this build.", {}});
        return;
    }

    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest apiReq{QUrl(apiUrl)};
    apiReq.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    apiReq.setRawHeader("Accept", "application/json");
    if (!authToken.isEmpty())
        apiReq.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());

    QNetworkReply* apiReply = nam->get(apiReq);

    connect(apiReply, &QNetworkReply::finished, this,
            [this, apiReply, nam, emuId, installPath, authToken]() {
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
                QHash<QString, QString> assetDigests;
                for (const auto& a : assets) {
                    QJsonObject asset = a.toObject();
                    QString name = asset["name"].toString();
                    QString url = InstallerUtils::chooseAssetDownloadUrl(asset, !authToken.isEmpty());
                    QString digest = asset["digest"].toString();
                    assetNames.append(name);
                    assetUrls.insert(name, url);
                    if (digest.startsWith("sha256:", Qt::CaseInsensitive))
                        assetDigests.insert(name, digest.mid(7).toLower());
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
                QString sha256 = assetDigests.value(assetName);  // empty = skip verify
                qInfo() << "[Installer] Selected asset:" << assetName
                        << (sha256.isEmpty() ? "(no digest)" : "(digest provided)");

                // Done with the API NAM \u2014 the download phase creates its own.
                nam->deleteLater();
                startDirectDownload(assetName, downloadUrl, tagName, publishedAt,
                                    sha256, installPath, authToken);
            });
}

// ============================================================================
// Async download + extract (shared by GitHub-Releases path and adapter-direct path)
// ============================================================================

void EmulatorInstaller::startDirectDownload(const QString& assetName,
                                              const QString& downloadUrl,
                                              const QString& tagName,
                                              const QString& publishedAt,
                                              const QString& sha256,
                                              const QString& installPath,
                                              const QString& authToken) {
    QDir().mkpath(installPath);
    const QString tempFile = installPath + "/" + assetName;

    auto* nam = new QNetworkAccessManager(this);
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
    if (!authToken.isEmpty()) {
        dlReq.setRawHeader("Accept", "application/octet-stream");
        dlReq.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());
    }
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
            [this, dlReply, nam, file, tempFile, installPath, tagName, publishedAt, sha256]() {
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

                // Phase 2.5: Integrity check (no-op if no digest was provided
                // upstream). Hashing a 100MB file is ~200ms on M-series and
                // belongs on the worker thread together with extract.
                emit progress(-1.0, "Verifying", "Verifying download...");

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
                    [tempFile, installPath, tagName, publishedAt, sha256]() -> InstallResult {
                        if (!InstallerUtils::verifySha256(tempFile, sha256, "[Installer]")) {
                            QFile::remove(tempFile);
                            return InstallResult{
                                false,
                                "Integrity check failed (SHA256 mismatch).",
                                tagName, publishedAt};
                        }
                        return postDownload(tempFile, installPath, tagName, publishedAt);
                    });

                watcher->setFuture(future);
            });
}
