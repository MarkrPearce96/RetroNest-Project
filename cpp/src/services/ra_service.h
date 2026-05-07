#pragma once

#include "core/database.h"
#include "core/ra_client.h"
#include "ra_credentials.h"
#include <QMutex>
#include <QNetworkAccessManager>
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

    /** Exchange username+password for a libretro login token via RA's login2
     *  endpoint. The password is NEVER stored. On success, the token is
     *  persisted into RACredentials::loginToken and loginTokenChanged() is
     *  emitted. On failure, loginFailed() is emitted with a human-readable
     *  message.
     *
     *  Architecturally clean: future work can fan out the password to
     *  standalone-emulator RA login by adding a listener to loginTokenChanged()
     *  and an additional method here — no surgery required. */
    Q_INVOKABLE void loginWithPassword(const QString& username, const QString& password);

    bool hasCredentials() const { return m_creds.hasCredentials(); }
    Q_INVOKABLE bool hasLibretroToken() const { return m_creds.hasLibretroToken(); }
    QString username() const { return m_creds.username; }
    const RACredentials& credentials() const { return m_creds; }

    // Async data access — each request runs HTTP fetch on a worker thread
    // and emits a *Ready signal on completion. Never blocks the caller.
    void requestUserSummary();
    void requestUserGames();
    void requestGameDetail(int raGameId);
    void requestGameIdLookup(const QString& title, const QString& system = {});

    // Settings
    bool hardcoreMode() const;
    void setHardcoreMode(bool enabled);
    bool notifications() const;
    void setNotifications(bool enabled);
    bool soundEffects() const;
    void setSoundEffects(bool enabled);

    /** Returns true if this is the first RA-enabled launch for this emulator. Marks it as prompted. */
    bool needsEmulatorLoginPrompt(const QString& emuId);

public slots:
    /** Called by RcheevosRuntime via queued connection when an in-process
     *  achievement unlocks. Forwards to the achievementUnlocked signal
     *  that the toast UI listens to. */
    void notifyAchievementUnlocked(const QString& id, const QString& title,
                                   const QString& description);

signals:
    void loginCompleted(bool success, const QString& message);
    void signedOut();
    /** Emitted when loginWithPassword() succeeds and the token has been persisted. */
    void loginTokenChanged();
    /** Emitted when loginWithPassword() fails with a human-readable reason. */
    void loginFailed(const QString& message);
    void userSummaryReady(const QVariantMap& summary);
    void userGamesReady(const QVariantList& games);
    void gameDetailReady(int raGameId, const QVariantMap& detail);
    void gameIdLookupReady(const QString& title, int raGameId);
    void achievementUnlocked(const QString& id, const QString& title,
                             const QString& description);

private:
    Database* m_db;
    RAClient* m_client;
    RACredentials m_creds;
    QNetworkAccessManager m_loginNam;

    // Cached data for title lookup. Touched from worker threads (lookup +
    // pre-cache populator) and from the GUI thread, so guarded by m_cacheMutex.
    QMutex m_cacheMutex;
    QVector<RAClient::GameProgressEntry> m_cachedUserGames;
    QMap<int, QVector<RAClient::ConsoleGame>> m_consoleGames; // consoleId → games

    void preCacheGameLists();
    int matchRaGameIdSync(const QString& title, const QString& system);
};
