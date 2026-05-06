#include "ra_service.h"
#include "core/paths.h"

#include <QRegularExpression>
#include <QtConcurrent>
#include <QDebug>

RAService::RAService(Database* db, QObject* parent)
    : QObject(parent), m_db(db), m_client(new RAClient(this))
{
}

void RAService::loadCredentials() {
    m_creds.load();
    if (m_creds.hasCredentials()) {
        m_client->setCredentials(m_creds.username, m_creds.apiKey);
        preCacheGameLists();
    }
}

void RAService::login(const QString& username, const QString& apiKey) {
    QtConcurrent::run([this, username, apiKey]() {
        auto result = m_client->validateApiKey(username, apiKey);
        QMetaObject::invokeMethod(this, [this, result, username, apiKey]() {
            if (result.success) {
                m_creds.username = username;
                m_creds.apiKey = apiKey;
                m_creds.save();
                m_client->setCredentials(username, apiKey);
                emit loginCompleted(true, "Logged in as " + username);
                preCacheGameLists();
            } else {
                emit loginCompleted(false, result.errorMessage);
            }
        });
    });
}

void RAService::signOut() {
    m_creds.clearUser();
    m_client->setCredentials("", "");
    emit signedOut();
}

// ── Async Data Access ──

void RAService::requestUserSummary() {
    if (!m_creds.hasCredentials()) {
        emit userSummaryReady({});
        return;
    }
    QtConcurrent::run([this]() {
        auto us = m_client->fetchUserSummary();
        QVariantMap map;
        if (us.success) {
            map["username"] = us.username;
            map["totalPoints"] = us.totalPoints;
            map["softcorePoints"] = us.softcorePoints;
            map["rank"] = us.rank;
            map["userPic"] = us.userPic.isEmpty() ? ""
                : "https://media.retroachievements.org" + us.userPic;
            map["memberSince"] = us.memberSince;
            map["totalTruePoints"] = us.totalTruePoints;
            map["lastGameTitle"] = us.lastGameTitle;
            map["lastGameIcon"] = us.lastGameIcon.isEmpty() ? ""
                : "https://retroachievements.org" + us.lastGameIcon;
            map["lastGameId"] = us.lastGameId;
            map["recentGames"] = us.recentGames;
            map["recentAchievements"] = us.recentAchievements;
        }
        QMetaObject::invokeMethod(this, [this, map]() {
            emit userSummaryReady(map);
        });
    });
}

void RAService::requestUserGames() {
    if (!m_creds.hasCredentials()) {
        emit userGamesReady({});
        return;
    }
    QtConcurrent::run([this]() {
        auto games = m_client->fetchUserGames();
        {
            QMutexLocker lock(&m_cacheMutex);
            m_cachedUserGames = games;
        }

        QVariantList list;
        for (const auto& g : games) {
            QVariantMap map;
            map["raGameId"] = g.gameId;
            map["title"] = g.title;
            map["consoleName"] = g.consoleName;
            map["imageIcon"] = g.imageIcon;
            map["numAchievements"] = g.numAchievements;
            map["numAwarded"] = g.numAwarded;
            map["numAwardedHardcore"] = g.numAwardedHardcore;
            map["completionPct"] = g.completionPct;
            map["mastered"] = (g.numAwarded >= g.numAchievements && g.numAchievements > 0);
            list.append(map);
        }
        QMetaObject::invokeMethod(this, [this, list]() {
            emit userGamesReady(list);
        });
    });
}

void RAService::requestGameDetail(int raGameId) {
    if (!m_creds.hasCredentials()) {
        emit gameDetailReady(raGameId, {});
        return;
    }
    QtConcurrent::run([this, raGameId]() {
        auto detail = m_client->fetchGameDetail(raGameId);
        QVariantMap map;
        if (detail.gameId != 0) {
            map["gameId"] = detail.gameId;
            map["title"] = detail.title;
            map["consoleName"] = detail.consoleName;
            map["imageIcon"] = detail.imageIcon;
            map["numAchievements"] = detail.numAchievements;
            map["numAwarded"] = detail.numAwarded;
            map["numAwardedHardcore"] = detail.numAwardedHardcore;
            map["completionPct"] = detail.completionPct;

            QVariantList achList;
            for (const auto& ach : detail.achievements) {
                QVariantMap achMap;
                achMap["id"] = ach.id;
                achMap["title"] = ach.title;
                achMap["description"] = ach.description;
                achMap["points"] = ach.points;
                achMap["trueRatio"] = ach.trueRatio;
                achMap["badgeName"] = ach.badgeName;
                achMap["badgeUrl"] = "https://media.retroachievements.org/Badge/" + ach.badgeName + ".png";
                achMap["type"] = ach.type;
                achMap["earned"] = ach.earned;
                achMap["earnedHardcore"] = ach.earnedHardcore;
                achMap["earnedDate"] = ach.earnedDate;
                achList.append(achMap);
            }
            map["achievements"] = achList;
        }
        QMetaObject::invokeMethod(this, [this, raGameId, map]() {
            emit gameDetailReady(raGameId, map);
        });
    });
}

void RAService::requestGameIdLookup(const QString& title, const QString& system) {
    if (!m_creds.hasCredentials()) {
        emit gameIdLookupReady(title, 0);
        return;
    }
    QtConcurrent::run([this, title, system]() {
        int id = matchRaGameIdSync(title, system);
        QMetaObject::invokeMethod(this, [this, title, id]() {
            emit gameIdLookupReady(title, id);
        });
    });
}

int RAService::matchRaGameIdSync(const QString& title, const QString& system) {

    // Normalize a title for comparison: lowercase, standardize separators, remove fluff
    auto normalize = [](const QString& t) -> QString {
        static const QRegularExpression reParens("\\s*\\(.*?\\)");
        static const QRegularExpression reSpaces("\\s+");
        QString n = t.toLower().trimmed();
        n.replace(reParens, "");
        n.replace(" - ", ": ");
        n.replace("&", "and");
        n.replace(reSpaces, " ");
        return n.trimmed();
    };

    // Extra normalization: strip punctuation and convert roman numerals
    auto deepNormalize = [](const QString& t) -> QString {
        static const QRegularExpression reViii("\\bviii\\b");
        static const QRegularExpression reVii("\\bvii\\b");
        static const QRegularExpression reVi("\\bvi\\b");
        static const QRegularExpression reIv("\\biv\\b");
        static const QRegularExpression reV("\\bv\\b");
        static const QRegularExpression reIii("\\biii\\b");
        static const QRegularExpression reIi("\\bii\\b");
        static const QRegularExpression reNonAlnum("[^a-z0-9 ]");
        static const QRegularExpression reSpaces("\\s+");

        QString n = t;
        // Strip brand prefixes
        static const QStringList prefixes = {
            "disney-pixar ", "disney's ", "disney ", "pixar ",
            "tom clancy's ", "marvel's ", "lego ", "dreamworks ",
        };
        for (const auto& p : prefixes) {
            if (n.startsWith(p)) { n = n.mid(p.length()); break; }
        }
        // Roman numerals -> Arabic (common ones, word-boundary safe)
        n.replace(reViii, "8");
        n.replace(reVii, "7");
        n.replace(reVi, "6");
        n.replace(reIv, "4");
        n.replace(reV, "5");
        n.replace(reIii, "3");
        n.replace(reIi, "2");
        // Strip punctuation (keep letters, digits, spaces)
        n.replace(reNonAlnum, "");
        n.replace(reSpaces, " ");
        return n.trimmed();
    };

    QString norm = normalize(title);
    QString deepNorm = deepNormalize(norm);

    // Score how well two titles match. 0 = no match, higher = better.
    auto matchScore = [&norm, &deepNorm, &normalize, &deepNormalize](const QString& raTitle) -> int {
        QString rt = normalize(raTitle);
        // Exact match = best
        if (rt == norm) return 1000;
        // Close-length contains (within 5 chars)
        if (rt.length() > 3 && norm.length() > 3) {
            int lenDiff = qAbs(rt.length() - norm.length());
            if (lenDiff <= 5 && (rt.contains(norm) || norm.contains(rt)))
                return 900;
        }
        // Prefix match at ": " boundary — one title is a prefix of the other
        // e.g. "tomb raider ii" matches "tomb raider ii: starring lara croft"
        const QString& shorter = (rt.length() <= norm.length()) ? rt : norm;
        const QString& longer = (rt.length() <= norm.length()) ? norm : rt;
        if (shorter.length() >= 8 && longer.startsWith(shorter)) {
            if (longer.length() == shorter.length() ||
                longer.mid(shorter.length()).startsWith(": ")) {
                return 500 + shorter.length();
            }
        }
        // Long substring match — handles publisher prefixes like "Disney-Pixar"
        if (shorter.length() >= 15 && longer.contains(shorter)) {
            return 400 + shorter.length();
        }
        // Deep normalization: strip punctuation, roman numerals → arabic, brand prefixes
        QString rtDeep = deepNormalize(rt);
        if (rtDeep == deepNorm) return 350;
        // Deep prefix match
        const QString& dShorter = (rtDeep.length() <= deepNorm.length()) ? rtDeep : deepNorm;
        const QString& dLonger = (rtDeep.length() <= deepNorm.length()) ? deepNorm : rtDeep;
        if (dShorter.length() >= 8 && dLonger.startsWith(dShorter)) {
            return 300 + dShorter.length();
        }
        // Deep long substring
        if (dShorter.length() >= 15 && dLonger.contains(dShorter)) {
            return 200 + dShorter.length();
        }
        return 0;
    };

    int bestId = 0;
    int bestScore = 0;

    // 1. Check user's played games first. Snapshot under the lock; if empty,
    // fetch (HTTP) without holding the lock, then publish.
    QVector<RAClient::GameProgressEntry> userGamesSnapshot;
    {
        QMutexLocker lock(&m_cacheMutex);
        userGamesSnapshot = m_cachedUserGames;
    }
    if (userGamesSnapshot.isEmpty()) {
        userGamesSnapshot = m_client->fetchUserGames();
        QMutexLocker lock(&m_cacheMutex);
        m_cachedUserGames = userGamesSnapshot;
    }
    for (const auto& g : userGamesSnapshot) {
        int score = matchScore(g.title);
        if (score >= 1000) return g.gameId;
        if (score > bestScore) { bestScore = score; bestId = g.gameId; }
    }

    // 2. Check full console game list (fetch and cache per console)
    int consoleId = system.isEmpty() ? -1 : RAClient::raConsoleId(system);
    QList<int> consoleIds;
    if (consoleId > 0) {
        consoleIds = {consoleId};
    } else {
        consoleIds = RAClient::allConsoleIds();
    }

    for (int cid : consoleIds) {
        QVector<RAClient::ConsoleGame> consoleSnapshot;
        bool needFetch = false;
        {
            QMutexLocker lock(&m_cacheMutex);
            if (m_consoleGames.contains(cid))
                consoleSnapshot = m_consoleGames[cid];
            else
                needFetch = true;
        }
        if (needFetch) {
            consoleSnapshot = m_client->fetchConsoleGames(cid);
            QMutexLocker lock(&m_cacheMutex);
            m_consoleGames[cid] = consoleSnapshot;
        }
        for (const auto& g : consoleSnapshot) {
            int score = matchScore(g.title);
            if (score >= 1000) return g.gameId;
            if (score > bestScore) { bestScore = score; bestId = g.gameId; }
        }
    }

    return bestId;
}

// ── In-Process Achievement Unlock ──

void RAService::notifyAchievementUnlocked(const QString& id, const QString& title,
                                          const QString& description) {
    qInfo() << "[RAService] Achievement unlocked:" << id << title;
    emit achievementUnlocked(id, title, description);
}

// ── Settings ──

bool RAService::hardcoreMode() const { return m_creds.hardcoreMode; }
void RAService::setHardcoreMode(bool enabled) {
    m_creds.hardcoreMode = enabled;
    m_creds.save();
}

bool RAService::notifications() const { return m_creds.notifications; }
void RAService::setNotifications(bool enabled) {
    m_creds.notifications = enabled;
    m_creds.save();
}

bool RAService::soundEffects() const { return m_creds.soundEffects; }
void RAService::setSoundEffects(bool enabled) {
    m_creds.soundEffects = enabled;
    m_creds.save();
}

bool RAService::needsEmulatorLoginPrompt(const QString& emuId) {
    if (!m_creds.hasCredentials()) return false;
    if (m_creds.promptedEmulators.contains(emuId)) return false;
    m_creds.promptedEmulators.append(emuId);
    m_creds.save();
    return true;
}

void RAService::preCacheGameLists() {
    QtConcurrent::run([this]() {
        QList<int> consoleIds = RAClient::allConsoleIds();
        for (int cid : consoleIds) {
            {
                QMutexLocker lock(&m_cacheMutex);
                if (m_consoleGames.contains(cid)) continue;
            }
            auto games = m_client->fetchConsoleGames(cid);
            QMutexLocker lock(&m_cacheMutex);
            m_consoleGames[cid] = games;
        }
        bool userGamesNeeded = false;
        {
            QMutexLocker lock(&m_cacheMutex);
            userGamesNeeded = m_cachedUserGames.isEmpty();
        }
        if (userGamesNeeded) {
            auto games = m_client->fetchUserGames();
            QMutexLocker lock(&m_cacheMutex);
            m_cachedUserGames = games;
        }
    });
}
