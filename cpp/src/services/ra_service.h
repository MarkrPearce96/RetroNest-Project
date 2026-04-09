#pragma once

#include "core/database.h"
#include "core/ra_client.h"
#include "core/ra_credentials.h"
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class RAService : public QObject {
    Q_OBJECT

public:
    RAService(Database* db, QObject* parent = nullptr);

    void loadCredentials();

    // Login: validate API key
    void login(const QString& username, const QString& apiKey);
    void signOut();

    bool hasCredentials() const { return m_creds.hasCredentials(); }
    QString username() const { return m_creds.username; }
    const RACredentials& credentials() const { return m_creds; }

    // Data access — fetches from web API
    QVariantMap userSummary();
    QVariantList userGames();
    QVariantMap gameDetail(int raGameId);

    /** Find RA game ID by title match. Searches user's games first, then full console game lists. */
    int findRaGameId(const QString& title, const QString& system = {});

    // Settings
    bool hardcoreMode() const;
    void setHardcoreMode(bool enabled);
    bool notifications() const;
    void setNotifications(bool enabled);
    bool soundEffects() const;
    void setSoundEffects(bool enabled);

    /** Returns true if this is the first RA-enabled launch for this emulator. Marks it as prompted. */
    bool needsEmulatorLoginPrompt(const QString& emuId);

signals:
    void loginCompleted(bool success, const QString& message);
    void signedOut();

private:
    Database* m_db;
    RAClient* m_client;
    RACredentials m_creds;

    // Cached data for title lookup
    QVector<RAClient::GameProgressEntry> m_cachedUserGames;
    QMap<int, QVector<RAClient::ConsoleGame>> m_consoleGames; // consoleId → games

    void preCacheGameLists();
};
