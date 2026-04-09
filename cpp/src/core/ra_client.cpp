#include "ra_client.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>
#include <QUrl>

static const int HTTP_TIMEOUT_MS = 30000;

RAClient::RAClient(QObject* parent) : QObject(parent) {}

void RAClient::setCredentials(const QString& username, const QString& apiKey) {
    m_username = username;
    m_apiKey = apiKey;
}

// ── HTTP Helpers ──

QByteArray RAClient::httpGet(const QString& url, QAtomicInt* cancelFlag) {
    // Guard against calls during shutdown (no event loop available)
    if (!QCoreApplication::instance()) return {};

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkAccessManager nam;
    auto* reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(HTTP_TIMEOUT_MS);

    loop.exec();

    if (cancelFlag && cancelFlag->loadRelaxed()) {
        reply->abort();
        reply->deleteLater();
        return {};
    }

    if (!timer.isActive()) {
        qWarning() << "[RAClient] HTTP timeout:" << url;
        reply->abort();
        reply->deleteLater();
        return {};
    }
    timer.stop();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[RAClient] HTTP error:" << reply->errorString();
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

QJsonDocument RAClient::httpGetJson(const QString& url, QAtomicInt* cancelFlag) {
    QByteArray data = httpGet(url, cancelFlag);
    if (data.isEmpty()) return {};
    return QJsonDocument::fromJson(data);
}

QString RAClient::apiUrl(const QString& endpoint) const {
    return QString("https://retroachievements.org/API/%1?z=%2&y=%3")
        .arg(endpoint,
             QUrl::toPercentEncoding(m_username),
             QUrl::toPercentEncoding(m_apiKey));
}

// ── Validation ──

RAClient::ValidateResult RAClient::validateApiKey(const QString& username, const QString& apiKey) {
    QString url = QString("https://retroachievements.org/API/API_GetUserSummary.php?u=%1&z=%2&y=%3&g=0&a=0")
        .arg(QUrl::toPercentEncoding(username),
             QUrl::toPercentEncoding(username),
             QUrl::toPercentEncoding(apiKey));

    auto doc = httpGetJson(url);
    if (doc.isNull())
        return {false, "Network error — could not reach RetroAchievements."};

    QJsonObject obj = doc.object();
    if (!obj.contains("ID"))
        return {false, "Invalid API key or username."};

    ValidateResult result;
    result.success = true;
    result.totalPoints = obj["TotalPoints"].toVariant().toInt();
    result.softcorePoints = obj["TotalSoftcorePoints"].toVariant().toInt();
    result.rank = obj["Rank"].toVariant().toInt();
    return result;
}

// ── User Summary ──

RAClient::UserSummary RAClient::fetchUserSummary(QAtomicInt* cancelFlag) {
    QString url = apiUrl("API_GetUserSummary.php")
        + "&u=" + QUrl::toPercentEncoding(m_username)
        + "&g=5&a=10";

    auto doc = httpGetJson(url, cancelFlag);
    if (doc.isNull()) return {};

    QJsonObject obj = doc.object();

    if (!obj.contains("ID")) {
        qWarning() << "[RAClient] UserSummary: no ID field, response:"
                    << QJsonDocument(obj).toJson(QJsonDocument::Compact).left(500);
        return {};
    }

    UserSummary us;
    us.success = true;
    us.username = obj["User"].toString();
    // RA API returns some fields as strings, not ints
    us.totalPoints = obj["TotalPoints"].toVariant().toInt();
    us.softcorePoints = obj["TotalSoftcorePoints"].toVariant().toInt();
    us.rank = obj["Rank"].toVariant().toInt();
    us.userPic = obj["UserPic"].toString();
    us.memberSince = obj["MemberSince"].toString();
    us.totalTruePoints = obj["TotalTruePoints"].toVariant().toInt();

    // Parse RecentlyPlayed array
    QJsonArray recentPlayed = obj["RecentlyPlayed"].toArray();
    if (!recentPlayed.isEmpty()) {
        QJsonObject lastGame = recentPlayed[0].toObject();
        us.lastGameTitle = lastGame["Title"].toString();
        us.lastGameIcon = lastGame["ImageIcon"].toString();
        us.lastGameId = lastGame["GameID"].toInt();
    }
    for (const auto& val : recentPlayed) {
        QJsonObject g = val.toObject();
        QVariantMap game;
        game["gameId"] = g["GameID"].toInt();
        game["title"] = g["Title"].toString();
        game["consoleId"] = g["ConsoleID"].toInt();
        game["consoleName"] = g["ConsoleName"].toString();
        game["imageIcon"] = g["ImageIcon"].toString();
        game["numAchieved"] = g["NumAchieved"].toInt();
        game["numPossible"] = g["NumPossibleAchievements"].toInt();
        us.recentGames.append(game);
    }

    // Parse RecentAchievements object — keyed by game ID, each containing achievements
    QJsonObject recentAchObj = obj["RecentAchievements"].toObject();
    for (auto it = recentAchObj.begin(); it != recentAchObj.end(); ++it) {
        QJsonObject gameAchs = it.value().toObject();
        for (auto achIt = gameAchs.begin(); achIt != gameAchs.end(); ++achIt) {
            QJsonObject a = achIt.value().toObject();
            QVariantMap ach;
            ach["title"] = a["Title"].toString();
            ach["gameTitle"] = a["GameTitle"].toString();
            ach["badgeName"] = a["BadgeName"].toString();
            ach["date"] = a["DateAwarded"].toString();
            ach["points"] = a["Points"].toInt();
            ach["gameId"] = a["GameID"].toInt();
            us.recentAchievements.append(ach);
        }
    }

    return us;
}

// ── User Games ──

QVector<RAClient::GameProgressEntry> RAClient::fetchUserGames(QAtomicInt* cancelFlag) {
    QString url = apiUrl("API_GetUserCompletedGames.php")
        + "&u=" + QUrl::toPercentEncoding(m_username);

    auto doc = httpGetJson(url, cancelFlag);
    if (doc.isNull()) return {};

    QVector<GameProgressEntry> result;
    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        QJsonObject g = val.toObject();
        GameProgressEntry entry;
        entry.gameId = g["GameID"].toInt();
        entry.title = g["Title"].toString();
        entry.consoleName = g["ConsoleName"].toString();
        entry.imageIcon = g["ImageIcon"].toString();
        entry.numAchievements = g["MaxPossible"].toInt();
        entry.numAwarded = g["NumAwarded"].toInt();
        entry.numAwardedHardcore = g["NumAwardedHardcore"].toInt();
        if (entry.numAchievements > 0) {
            int pct = entry.numAwarded * 100 / entry.numAchievements;
            entry.completionPct = QString::number(pct) + "%";
        }
        result.append(entry);
    }
    return result;
}

// ── Game Detail ──

RAClient::GameDetail RAClient::fetchGameDetail(int gameId, QAtomicInt* cancelFlag) {
    QString url = apiUrl("API_GetGameInfoAndUserProgress.php")
        + "&g=" + QString::number(gameId)
        + "&u=" + QUrl::toPercentEncoding(m_username);

    auto doc = httpGetJson(url, cancelFlag);
    if (doc.isNull()) return {};

    QJsonObject obj = doc.object();

    GameDetail detail;
    detail.gameId = obj["ID"].toInt();
    detail.title = obj["Title"].toString();
    detail.consoleName = obj["ConsoleName"].toString();
    detail.imageIcon = obj["ImageIcon"].toString();
    detail.numAchievements = obj["NumAchievements"].toInt();
    detail.numAwarded = obj["NumAwardedToUser"].toInt();
    detail.numAwardedHardcore = obj["NumAwardedToUserHardcore"].toInt();
    if (detail.numAchievements > 0) {
        int pct = detail.numAwarded * 100 / detail.numAchievements;
        detail.completionPct = QString::number(pct) + "%";
    }

    // Parse Achievements object (keyed by achievement ID)
    QJsonObject achObj = obj["Achievements"].toObject();
    int earnedCount = 0, earnedHardcore = 0;
    for (auto it = achObj.begin(); it != achObj.end(); ++it) {
        QJsonObject a = it.value().toObject();
        Achievement ach;
        ach.id = a["ID"].toInt();
        ach.title = a["Title"].toString();
        ach.description = a["Description"].toString();
        ach.points = a["Points"].toInt();
        ach.trueRatio = a["TrueRatio"].toInt();
        ach.badgeName = a["BadgeName"].toString();
        int achType = a["type"].toInt();
        ach.type = (achType == 0) ? "core" : "unofficial";

        if (!a["DateEarnedHardcore"].toString().isEmpty()) {
            ach.earnedHardcore = true;
            ach.earned = true;
            ach.earnedDate = a["DateEarnedHardcore"].toString();
            earnedHardcore++;
            earnedCount++;
        } else if (!a["DateEarned"].toString().isEmpty()) {
            ach.earned = true;
            ach.earnedDate = a["DateEarned"].toString();
            earnedCount++;
        }

        detail.achievements.append(ach);
    }

    detail.numAwarded = earnedCount;
    detail.numAwardedHardcore = earnedHardcore;

    return detail;
}

// ── Console Game List ──

QVector<RAClient::ConsoleGame> RAClient::fetchConsoleGames(int consoleId, QAtomicInt* cancelFlag) {
    QString url = apiUrl("API_GetGameList.php")
        + "&i=" + QString::number(consoleId);

    auto doc = httpGetJson(url, cancelFlag);
    if (doc.isNull()) return {};

    QVector<ConsoleGame> games;
    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        QJsonObject g = val.toObject();
        ConsoleGame game;
        game.gameId = g["ID"].toVariant().toInt();
        game.title = g["Title"].toString();
        game.numAchievements = g["NumAchievements"].toVariant().toInt();
        if (game.gameId > 0 && game.numAchievements > 0)
            games.append(game);
    }

    qInfo() << "[RAClient] Fetched" << games.size() << "games with achievements for console" << consoleId;
    return games;
}

// ── Console ID Mapping ──

static const QMap<QString, int>& consoleIdMapping() {
    static const QMap<QString, int> mapping = {
        {"psx", 12},
        {"ps2", 21},
        {"psp", 41},
    };
    return mapping;
}

int RAClient::raConsoleId(const QString& systemId) {
    return consoleIdMapping().value(systemId, -1);
}

QList<int> RAClient::allConsoleIds() {
    return consoleIdMapping().values();
}
