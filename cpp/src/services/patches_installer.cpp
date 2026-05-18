#include "patches_installer.h"
#include "patches_sidecar.h"
#include "installer_utils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent>

PatchesInstaller::PatchesInstaller(QObject* parent) : QObject(parent) {}

bool PatchesInstaller::isFetchNeeded(const QString& resourcesDir) const {
    if (resourcesDir.isEmpty()) return false;
    const QFileInfo dirInfo(resourcesDir);
    if (!dirInfo.exists()) return false;

    const QString zipPath = resourcesDir + "/patches.zip";
    const QString sidecarPath = zipPath + ".version";

    const QFileInfo zipInfo(zipPath);
    if (!zipInfo.exists()) return true;  // zip missing → fetch

    const auto sidecar = PatchesSidecar::read(sidecarPath);
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (sidecar.has_value() && !sidecar->installedAt.isEmpty()) {
        const QDateTime installed = QDateTime::fromString(
            sidecar->installedAt, Qt::ISODate);
        if (!installed.isValid()) return true;  // malformed = treat as stale
        return installed.secsTo(now) > kStaleAgeSeconds;
    }

    // Zip present, sidecar absent or unusable → user-placed file.
    // Respect it until zip mtime crosses staleness threshold.
    const QDateTime zipMtime = zipInfo.lastModified().toUTC();
    return zipMtime.secsTo(now) > kStaleAgeSeconds;
}

void PatchesInstaller::fetchAsync(const QString& resourcesDir, bool force) {
    QtConcurrent::run([this, resourcesDir, force]() {
        runFetch(resourcesDir, force);
    });
}

void PatchesInstaller::runFetch(const QString& resourcesDir, bool force) {
    if (!force && !isFetchNeeded(resourcesDir)) {
        const auto sc = PatchesSidecar::read(resourcesDir + "/patches.zip.version");
        emit finished(true, "Patches already up to date",
                      sc.has_value() ? sc->tag : QString{});
        return;
    }

    emit progress(0.0, "Fetching release info");
    ReleaseInfo info = fetchReleaseInfo();
    if (!info.ok) {
        emit finished(false, info.errorMessage, {});
        return;
    }

    // Tag short-circuit: if sidecar matches latest tag AND zip is present,
    // no need to re-download.
    const QString zipPath = resourcesDir + "/patches.zip";
    const QString sidecarPath = zipPath + ".version";
    if (QFile::exists(zipPath)) {
        const auto sc = PatchesSidecar::read(sidecarPath);
        if (sc.has_value() && sc->tag == info.tagName && !info.tagName.isEmpty()) {
            // Refresh installed_at so staleness clock resets.
            PatchesSidecar refreshed = *sc;
            refreshed.installedAt = QDateTime::currentDateTimeUtc()
                                        .toString(Qt::ISODate);
            PatchesSidecar::write(sidecarPath, refreshed);
            emit finished(true, "Patches already up to date", info.tagName);
            return;
        }
    }

    emit progress(0.1, "Downloading patches.zip");
    const QString tmpPath = zipPath + ".tmp";
    QDir().mkpath(resourcesDir);
    if (!downloadTo(info.downloadUrl, tmpPath)) {
        QFile::remove(tmpPath);
        emit finished(false, "Download failed", info.tagName);
        return;
    }

    if (!InstallerUtils::verifySha256(tmpPath, info.sha256, "[Patches]")) {
        QFile::remove(tmpPath);
        emit finished(false, "SHA256 verification failed", info.tagName);
        return;
    }

    QFile::remove(zipPath);
    if (!QFile::rename(tmpPath, zipPath)) {
        QFile::remove(tmpPath);
        emit finished(false, "Could not finalize patches.zip on disk",
                      info.tagName);
        return;
    }

    PatchesSidecar updated;
    updated.tag = info.tagName;
    updated.publishedAt = info.publishedAt;
    updated.installedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    updated.sha256 = info.sha256;
    PatchesSidecar::write(sidecarPath, updated);

    emit finished(true,
                  QString("Patches updated to %1").arg(info.tagName),
                  info.tagName);
}

PatchesInstaller::ReleaseInfo PatchesInstaller::fetchReleaseInfo() const {
    ReleaseInfo info;
    const QString apiUrl =
        "https://api.github.com/repos/PCSX2/pcsx2_patches/releases/latest";
    const QByteArray body =
        InstallerUtils::httpGet(apiUrl, 30000, "[Patches]");
    if (body.isEmpty()) {
        info.errorMessage = "Could not reach GitHub";
        return info;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        info.errorMessage = "Invalid release JSON from GitHub";
        return info;
    }
    const QJsonObject release = doc.object();
    info.tagName = release["tag_name"].toString();
    info.publishedAt = release["published_at"].toString();

    for (const auto& a : release["assets"].toArray()) {
        const QJsonObject asset = a.toObject();
        if (asset["name"].toString() == "patches.zip") {
            info.downloadUrl = asset["browser_download_url"].toString();
            const QString digest = asset["digest"].toString();
            if (digest.startsWith("sha256:", Qt::CaseInsensitive))
                info.sha256 = digest.mid(7).toLower();
            info.ok = !info.downloadUrl.isEmpty();
            if (!info.ok) info.errorMessage = "Asset patches.zip has no download URL";
            return info;
        }
    }
    info.errorMessage = "No patches.zip asset in latest release";
    return info;
}

bool PatchesInstaller::downloadTo(const QString& url, const QString& destPath) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam.get(req);

    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reply->abort();
        reply->deleteLater();
        return false;
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::readyRead, &out, [&]() {
        out.write(reply->readAll());
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, this,
                     [this](qint64 r, qint64 t) {
        if (t > 0) emit progress(0.1 + 0.9 * (qreal(r) / qreal(t)),
                                  "Downloading patches.zip");
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // Cmd+Q during fetch — abort cleanly so the threadpool can join.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, reply,
                     &QNetworkReply::abort);

    loop.exec();

    out.write(reply->readAll());
    out.close();

    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) qWarning() << "[Patches] Download error:" << reply->errorString();
    reply->deleteLater();
    return ok;
}
