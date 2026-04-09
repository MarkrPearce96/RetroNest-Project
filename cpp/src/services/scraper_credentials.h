#pragma once

#include <QString>
#include <QJsonObject>

/**
 * ScraperCredentials — wraps JSON I/O for ScreenScraper credentials.
 * Single file at {config}/scraper.json.
 * Dev credentials come from compile-time defines; only user credentials are persisted.
 */
class ScraperCredentials {
public:
    QString devId;
    QString devPassword;
    QString softname;
    QString ssId;
    QString ssPassword;

    /** Load from disk. Dev credentials set from compile-time defines; user credentials from JSON. */
    bool load();

    /** Save to disk (user credentials only). Returns true on success. */
    bool save() const;

    /** Clear user credentials and remove the JSON file. */
    void clearUser();

    /** True if user credentials are set. */
    bool hasUserCredentials() const { return !ssId.isEmpty(); }

private:
    static QString filePath();
};
