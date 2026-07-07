#include "theme_manager.h"
#include "core/paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QVersionNumber>
#include <QDebug>
#include <algorithm>

// theme.json contract — validated at scan so a broken theme is rejected
// with a log line instead of failing at first navigation. Unknown keys
// warn (forward-compat typo net, same policy as the manifest loader).
static const QSet<QString> kKnownThemeKeys = {
    "name", "version", "author", "description", "preview", "pages",
    "minAppVersion"
};

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
}

void ThemeManager::scanThemes(const QString& themesDir) {
    m_themesDir = themesDir;

    QDir dir(themesDir);
    if (!dir.exists()) {
        qWarning() << "[ThemeManager] Themes directory not found:" << themesDir;
        emit themesChanged();
        return;
    }

    QSet<QString> seenIds;
    for (const auto& existing : m_themes)
        seenIds.insert(existing.id);

    for (const auto& entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString themeJsonPath = entry.absoluteFilePath() + "/theme.json";
        QFile file(themeJsonPath);
        if (!file.open(QIODevice::ReadOnly)) continue;

        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
        file.close();

        if (doc.isNull()) {
            qWarning() << "[ThemeManager] Failed to parse" << themeJsonPath << ":" << parseErr.errorString();
            continue;
        }

        const QJsonObject obj = doc.object();

        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (!kKnownThemeKeys.contains(it.key()))
                qWarning() << "[ThemeManager] Theme" << entry.fileName()
                           << "theme.json has unknown key" << it.key();
        }

        const QString minApp = obj["minAppVersion"].toString();
        if (!minApp.isEmpty()) {
            const auto required = QVersionNumber::fromString(minApp);
            const auto current  = QVersionNumber::fromString(
                QCoreApplication::applicationVersion());
            if (!required.isNull() && !current.isNull() && current < required) {
                qWarning() << "[ThemeManager] Theme" << entry.fileName()
                           << "requires app" << minApp << "but this is"
                           << current.toString() << "— skipping";
                continue;
            }
        }

        ThemeInfo info;
        info.id          = entry.fileName();
        info.name        = obj["name"].toString(info.id);
        info.version     = obj["version"].toString("1.0");
        info.author      = obj["author"].toString("Unknown");
        info.description = obj["description"].toString();
        info.path        = entry.absoluteFilePath();

        const QString preview = obj["preview"].toString();
        if (!preview.isEmpty())
            info.previewImage = info.path + "/" + preview;

        const QJsonObject pages = obj["pages"].toObject();
        for (auto it = pages.begin(); it != pages.end(); ++it)
            info.pages.insert(it.key(), it.value().toString());

        if (!info.pages.contains("systemBrowser") || !info.pages.contains("gameList")) {
            qWarning() << "[ThemeManager] Theme" << info.id << "missing required pages (systemBrowser, gameList)";
            continue;
        }

        // Every declared page file must exist NOW — resolve() failing at
        // navigation time would leave dead pages mid-session.
        bool pagesOk = true;
        for (auto it = info.pages.cbegin(); it != info.pages.cend(); ++it) {
            if (!QFile::exists(info.path + "/" + it.value())) {
                qWarning() << "[ThemeManager] Theme" << info.id << "page"
                           << it.key() << "file missing:" << it.value();
                pagesOk = false;
            }
        }
        if (!pagesOk) continue;

        if (seenIds.contains(info.id)) continue;  // duplicate theme ID
        seenIds.insert(info.id);

        qInfo() << "[ThemeManager] Found theme:" << info.name << "(" << info.id << ")";
        m_themes.append(info);
    }

    if (m_themes.isEmpty())
        qCritical() << "[ThemeManager] No valid themes found — UI will be blank";

    const QString previous = m_currentThemeId;

    // Prefer the persisted choice as soon as a scan pass makes it available
    // (themes arrive in two passes: user dir, then bundled), else fall back
    // to the first theme found.
    const QString saved = Paths::loadSavedTheme();
    if (!saved.isEmpty() && m_currentThemeId != saved) {
        const bool savedFound = std::any_of(m_themes.cbegin(), m_themes.cend(),
            [&saved](const ThemeInfo& t) { return t.id == saved; });
        if (savedFound)
            m_currentThemeId = saved;
    }
    if (m_currentThemeId.isEmpty() && !m_themes.isEmpty())
        m_currentThemeId = m_themes.first().id;

    if (m_currentThemeId != previous)
        emit currentThemeChanged();
    emit themesChanged();
}

QVariantList ThemeManager::availableThemes() const {
    QVariantList list;
    for (const auto& t : m_themes) {
        QVariantMap map;
        map["id"]          = t.id;
        map["name"]        = t.name;
        map["version"]     = t.version;
        map["author"]      = t.author;
        map["description"] = t.description;
        map["previewImage"] = t.previewImage;
        list.append(map);
    }
    return list;
}

QString ThemeManager::currentThemeId() const {
    return m_currentThemeId;
}

void ThemeManager::setCurrentThemeId(const QString& id) {
    if (m_currentThemeId == id) return;

    const bool found = std::any_of(m_themes.cbegin(), m_themes.cend(),
        [&id](const ThemeInfo& t) { return t.id == id; });
    if (!found) {
        qWarning() << "[ThemeManager] Theme not found:" << id;
        return;
    }

    m_currentThemeId = id;
    Paths::saveTheme(id);
    emit currentThemeChanged();
}

QUrl ThemeManager::resolve(const QString& pageName) const {
    const ThemeInfo* theme = currentTheme();
    if (!theme) {
        qWarning() << "[ThemeManager] No active theme, cannot resolve:" << pageName;
        return {};
    }

    QString filename = theme->pages.value(pageName);
    if (filename.isEmpty()) {
        qWarning() << "[ThemeManager] Page not found in theme" << theme->id << ":" << pageName;
        return {};
    }

    QString fullPath = theme->path + "/" + filename;
    if (!QFile::exists(fullPath)) {
        qWarning() << "[ThemeManager] Theme file not found:" << fullPath;
        return {};
    }

    return QUrl::fromLocalFile(fullPath);
}

QVariantMap ThemeManager::themeInfo() const {
    const ThemeInfo* theme = currentTheme();
    if (!theme) return {};

    QVariantMap map;
    map["id"]          = theme->id;
    map["name"]        = theme->name;
    map["version"]     = theme->version;
    map["author"]      = theme->author;
    map["description"] = theme->description;
    return map;
}

const ThemeInfo* ThemeManager::currentTheme() const {
    auto it = std::find_if(m_themes.cbegin(), m_themes.cend(),
        [this](const ThemeInfo& t) { return t.id == m_currentThemeId; });
    if (it != m_themes.cend())
        return &(*it);
    return m_themes.isEmpty() ? nullptr : &m_themes.first();
}
