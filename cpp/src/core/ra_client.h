#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QAtomicInt>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>

/**
 * RAClient — RetroAchievements web API client.
 * Uses the public web API with username + API key authentication.
 * API key is obtained from retroachievements.org/settings.
 */
class RAClient : public QObject {
    Q_OBJECT

public:
    explicit RAClient(QObject* parent = nullptr);

    void setCredentials(const QString& username, const QString& apiKey);

    // Validation — test API key with a lightweight call
    struct ValidateResult {
        bool success = false;
        QString errorMessage;
        int totalPoints = 0;
        int softcorePoints = 0;
        int rank = 0;
    };
    ValidateResult validateApiKey(const QString& username, const QString& apiKey);

    // User profile + recent data
    struct UserSummary {
        bool success = false;
        QString username;
        int totalPoints = 0;
        int softcorePoints = 0;
        int rank = 0;
        QString userPic;         // avatar URL path (e.g. "/UserPic/Name.png")
        QString memberSince;     // join date
        int totalTruePoints = 0; // rarity-weighted points
        QString lastGameTitle;   // last played game title
        QString lastGameIcon;    // last played game icon path
        int lastGameId = 0;      // last played game ID
        QVariantList recentGames;        // [{gameId, title, consoleId, consoleName, numAchieved, numPossible, ...}]
        QVariantList recentAchievements; // [{title, gameTitle, badgeName, date, points, ...}]
    };
    UserSummary fetchUserSummary(QAtomicInt* cancelFlag = nullptr);

    // All games user has played with progress
    struct GameProgressEntry {
        int gameId = 0;
        QString title;
        QString consoleName;
        QString imageIcon;
        int numAchievements = 0;
        int numAwarded = 0;
        int numAwardedHardcore = 0;
        QString completionPct;
    };
    QVector<GameProgressEntry> fetchUserGames(QAtomicInt* cancelFlag = nullptr);

    // Per-game achievement list with unlock status
    struct Achievement {
        int id = 0;
        QString title;
        QString description;
        int points = 0;
        int trueRatio = 0;
        QString badgeName;
        QString type;
        bool earned = false;
        bool earnedHardcore = false;
        QString earnedDate;
    };

    struct GameDetail {
        int gameId = 0;
        QString title;
        QString consoleName;
        QString imageIcon;
        int numAchievements = 0;
        int numAwarded = 0;
        int numAwardedHardcore = 0;
        QString completionPct;
        QVector<Achievement> achievements;
    };
    GameDetail fetchGameDetail(int gameId, QAtomicInt* cancelFlag = nullptr);

    // Console game list (for title-based lookup)
    struct ConsoleGame {
        int gameId = 0;
        QString title;
        int numAchievements = 0;
    };
    QVector<ConsoleGame> fetchConsoleGames(int consoleId, QAtomicInt* cancelFlag = nullptr);

    // Console ID mapping
    static int raConsoleId(const QString& systemId);
    static QList<int> allConsoleIds();

    // Public for badge downloads
    QByteArray httpGet(const QString& url, QAtomicInt* cancelFlag = nullptr);

private:
    QJsonDocument httpGetJson(const QString& url, QAtomicInt* cancelFlag = nullptr);
    QString apiUrl(const QString& endpoint) const;

    QString m_username;
    QString m_apiKey;
};
