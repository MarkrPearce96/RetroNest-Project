#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <rc_client.h>
#include "core_loader.h"

/**
 * In-process RetroAchievements client. Lives on the core thread; emits
 * achievement-unlock signals queued back to RAService on the main thread.
 *
 * HTTP layer: real network integration via QNetworkAccessManager (constructed
 * on the main thread). The httpHandler is invoked from the core thread and
 * hops network calls to the main thread via QMetaObject::invokeMethod with
 * QueuedConnection. rc_client callbacks are safe to invoke from any thread.
 */
class RcheevosRuntime : public QObject {
    Q_OBJECT
public:
    explicit RcheevosRuntime(QObject* parent = nullptr);
    ~RcheevosRuntime() override;

    /** Begin a session for the loaded game. Initiates async login + game-load
     *  chain via rc_client. Returns false (and emits loginRequired) if no
     *  credentials are available. m_inSession is set true only after both
     *  login and game-load succeed. */
    bool beginSession(const CoreSymbols& syms,
                      const QString& romPath,
                      int raConsoleId,
                      const QString& username,
                      const QString& token,
                      bool hardcore);
    void endSession();
    /** Per-frame tick. Cheap when no session is active or RA is disabled. */
    void frame();

    /** Propagate pref changes from the settings UI into the live rc_client. */
    void setHardcore(bool on);
    void setEnabled(bool on);

signals:
    void achievementUnlocked(const QString& id, const QString& title,
                             const QString& description);
    void loginRequired();

private:
    // Static callbacks registered with rc_client — declared here so they
    // can access private members directly.
    static void httpHandler(const rc_api_request_t* request,
                            rc_client_server_callback_t callback,
                            void* callback_data, rc_client_t* client);
    static uint32_t readMemoryHandler(uint32_t address, uint8_t* buffer,
                                      uint32_t num_bytes, rc_client_t* client);
    static void eventHandler(const rc_client_event_t* ev, rc_client_t* client);
    static void loginCallback(int result, const char* errorMessage,
                              rc_client_t* client, void* userdata);
    static void loadGameCallback(int result, const char* errorMessage,
                                 rc_client_t* client, void* userdata);

    rc_client_t* m_client = nullptr;
    bool m_inSession = false;
    bool m_enabled = true;

    // Pending values stored so login/load-game chained callbacks can access them.
    QString m_pendingRomPath;
    int m_pendingConsoleId = 0;

    // QNetworkAccessManager is not thread-safe; it must be used on its
    // construction thread (main thread). Constructed here on the main thread.
    QNetworkAccessManager m_nam;
};
