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

// ── Data Access ──

QVariantMap RAService::userSummary() {
    if (!m_creds.hasCredentials()) return {};

    auto us = m_client->fetchUserSummary();
    if (!us.success) return {};

    QVariantMap map;
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

    return map;
}

QVariantList RAService::userGames() {
    if (!m_creds.hasCredentials()) return {};

    auto games = m_client->fetchUserGames();
    m_cachedUserGames = games;

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
    return list;
}

QVariantMap RAService::gameDetail(int raGameId) {
    if (!m_creds.hasCredentials()) return {};

    auto detail = m_client->fetchGameDetail(raGameId);
    if (detail.gameId == 0) return {};

    QVariantMap map;
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

    return map;
}

int RAService::findRaGameId(const QString& title, const QString& system) {
    if (!m_creds.hasCredentials()) return 0;

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

    // 1. Check user's played games first (fast, already cached)
    if (m_cachedUserGames.isEmpty()) {
        m_cachedUserGames = m_client->fetchUserGames();
    }
    for (const auto& g : m_cachedUserGames) {
        int score = matchScore(g.title);
        if (score >= 1000) return g.gameId;  // exact match, return immediately
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
        if (!m_consoleGames.contains(cid)) {
            m_consoleGames[cid] = m_client->fetchConsoleGames(cid);
        }
        for (const auto& g : m_consoleGames[cid]) {
            int score = matchScore(g.title);
            if (score >= 1000) return g.gameId;  // exact match, return immediately
            if (score > bestScore) { bestScore = score; bestId = g.gameId; }
        }
    }

    return bestId;
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
            if (m_consoleGames.contains(cid)) continue;
            auto games = m_client->fetchConsoleGames(cid);
            QMetaObject::invokeMethod(this, [this, cid, games]() {
                m_consoleGames[cid] = games;
            });
        }
        if (m_cachedUserGames.isEmpty()) {
            auto games = m_client->fetchUserGames();
            QMetaObject::invokeMethod(this, [this, games]() {
                m_cachedUserGames = games;
            });
        }
    });
}
