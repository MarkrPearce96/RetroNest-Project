#pragma once

#include <QString>
#include <QStringList>

/**
 * RACredentials — wraps JSON I/O for RetroAchievements credentials + settings.
 * File at {config}/retroachievements.json.
 * Single owner of all RA persistent state.
 */
class RACredentials {
public:
    QString username;
    QString apiKey;        // web API key from retroachievements.org/settings
    QString loginToken;   // libretro/rcheevos token from login2 endpoint; never log this

    // Settings (persisted alongside credentials)
    bool hardcoreMode = false;
    bool notifications = true;
    bool soundEffects = true;
    QStringList promptedEmulators;

    /** Load from disk. Returns true if file was read (even if empty). */
    bool load();

    /** Save to disk. Returns true on success. */
    bool save() const;

    /** Clear credentials and remove the JSON file. */
    void clearUser();

    /** True if web-API-key-based features are available. */
    bool hasCredentials() const { return !username.isEmpty() && !apiKey.isEmpty(); }

    /** True if the libretro/rcheevos session token is available. */
    bool hasLibretroToken() const { return !username.isEmpty() && !loginToken.isEmpty(); }

private:
    static QString filePath();
};
