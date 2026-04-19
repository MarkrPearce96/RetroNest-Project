#pragma once

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QTimer>

/**
 * GitHubClient — minimal helper for GitHub API calls.
 * All methods are synchronous with timeouts. Use from background threads only.
 */
namespace GitHubClient {

struct LatestRelease {
    QString tag;          // e.g. "v2.6.3", or "latest" for rolling releases
    QString publishedAt;  // ISO 8601 — unique per release, even when tag is rolling
};

/**
 * Fetch the latest release info for a GitHub repo.
 * Returns an empty struct on error or timeout.
 */
inline LatestRelease fetchLatestRelease(const QString& githubRepo, int timeoutMs = 30000) {
    QString apiUrl = "https://api.github.com/repos/" + githubRepo + "/releases/latest";

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(apiUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(timeoutMs);
    loop.exec();

    LatestRelease info;
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[GitHubClient] Request failed for" << githubRepo
                    << ":" << reply->errorString();
    } else {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "[GitHubClient] JSON parse error for" << githubRepo
                        << ":" << parseError.errorString();
        } else {
            const QJsonObject obj = doc.object();
            info.tag = obj["tag_name"].toString();
            info.publishedAt = obj["published_at"].toString();
        }
    }

    reply->deleteLater();
    return info;
}

} // namespace GitHubClient
