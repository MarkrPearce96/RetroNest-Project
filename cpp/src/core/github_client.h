#pragma once

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <algorithm>

/**
 * GitHubClient — minimal helper for GitHub API calls.
 * All methods are synchronous with timeouts. Use from background threads only.
 */
namespace GitHubClient {

struct LatestRelease {
    QString tag;          // e.g. "v2.6.3", or "latest" for rolling releases
    QString publishedAt;  // ISO 8601 — unique per release, even when tag is rolling
};

namespace detail {

inline QByteArray httpGetJson(const QString& url, int timeoutMs) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
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

    QByteArray body;
    if (reply->error() != QNetworkReply::NoError)
        qWarning() << "[GitHubClient] Request failed:" << url << reply->errorString();
    else
        body = reply->readAll();
    reply->deleteLater();
    return body;
}

} // namespace detail

/**
 * Fetch the latest stable tag matching `pattern` from a repo's tag list.
 * Tags are sorted via the same captured-groups approach used by callers
 * (e.g. Dolphin's "2603a" beats "2603" beats "2512" via numeric prefix
 * then alpha suffix). Pass a QRegularExpression that captures the prefix
 * in group 1 and the optional suffix in group 2; the helper sorts by
 * (prefix-as-int DESC, suffix DESC) and returns the top match.
 *
 * Returns an empty string on error, no matches, or timeout.
 *
 * Example: `fetchLatestStableTag("dolphin-emu/dolphin", QRegularExpression("^(\\d{4})([a-z]?)$"))`
 *          returns "2603a".
 */
inline QString fetchLatestStableTag(const QString& githubRepo,
                                     const QRegularExpression& pattern,
                                     int perPage = 30,
                                     int timeoutMs = 30000) {
    const QString url = QString("https://api.github.com/repos/%1/tags?per_page=%2")
                            .arg(githubRepo).arg(perPage);
    QByteArray body = detail::httpGetJson(url, timeoutMs);
    if (body.isEmpty()) return {};

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning() << "[GitHubClient] Tags JSON parse error for" << githubRepo
                    << ":" << parseError.errorString();
        return {};
    }

    struct ParsedTag { QString name; int prefix = 0; QString suffix; };
    QVector<ParsedTag> matches;
    for (const QJsonValue& v : doc.array()) {
        QString name = v.toObject().value("name").toString();
        QRegularExpressionMatch m = pattern.match(name);
        if (!m.hasMatch()) continue;
        ParsedTag p;
        p.name = name;
        p.prefix = m.captured(1).toInt();
        p.suffix = m.captured(2);
        matches.append(p);
    }
    if (matches.isEmpty()) {
        qWarning() << "[GitHubClient] No tags matched pattern for" << githubRepo;
        return {};
    }
    std::sort(matches.begin(), matches.end(),
              [](const ParsedTag& a, const ParsedTag& b) {
                  if (a.prefix != b.prefix) return a.prefix > b.prefix;
                  return a.suffix > b.suffix;
              });
    return matches.first().name;
}

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
