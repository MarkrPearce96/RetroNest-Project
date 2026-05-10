#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <rc_client.h>
#include <rc_libretro.h>
#include "core_loader.h"

struct retro_memory_map;

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
     *  login and game-load succeed.
     *
     *  `memoryMap` may be nullptr (or a zero-descriptor map): rc_libretro
     *  falls back to the core's retro_get_memory_data callbacks based on
     *  console_id. Cores that send SET_MEMORY_MAPS (e.g. mGBA) need it for
     *  achievements that read outside SYSTEM_RAM (e.g. GBA IWRAM). */
    bool beginSession(const CoreSymbols& syms,
                      const QString& romPath,
                      int raConsoleId,
                      const QString& username,
                      const QString& token,
                      bool hardcore,
                      bool encore,
                      const retro_memory_map* memoryMap);

    /** Toggle encore mode on the live rc_client. Mid-session changes
     *  are honored — rcheevos re-evaluates the achievement set. */
    void setEncore(bool on);

    /** Snapshot of the current core-category achievement list as a
     *  QVariantList suitable for QML binding. Each entry is a
     *  QVariantMap with: id, title, description, points, earned,
     *  badgeUrl. Built from rc_client's in-memory state — no network
     *  call. Returns empty when no game is loaded.
     *
     *  Used by InGameAchievementsPopup for libretro games to avoid
     *  the redundant RA web API call (and the count-mismatch bug
     *  it produces vs the game-start banner). External-emulator
     *  games still go through the web API path. */
    QVariantList achievementListVariants();
    /** True once the rcheevos session is fully active (login + load
     *  callbacks both succeeded). False during the load window. */
    bool isInSession() const { return m_inSession; }
    void endSession();
    /** Per-frame tick. Cheap when no session is active or RA is disabled. */
    void frame();

    /** Propagate pref changes from the settings UI into the live rc_client. */
    void setHardcore(bool on);
    void setEnabled(bool on);

signals:
    void achievementUnlocked(const QString& id, const QString& title,
                             const QString& description,
                             const QString& imageUrl);
    /** Generic "show me as a toast" signal, used for the rcheevos events
     *  that don't deserve their own dedicated path: game-start session
     *  banner, game-mastered celebration, server-error notice, hardcore
     *  reset notice. Forwarded through GameSession → RAService →
     *  AppController → QML so QML can render via AchievementToast. */
    void raInfoToast(const QString& header, const QString& title,
                     const QString& description, const QString& imageUrl,
                     int durationMs);
    /** Persistent indicator-bar updates: challenge-active chips,
     *  progress-tracking chips, and connection-status banner. `kind`
     *  matches the rc_client event-type integer (5=ChallengeShow,
     *  6=ChallengeHide, 7=ProgressShow, 8=ProgressHide,
     *  9=ProgressUpdate, 17=Disconnected, 18=Reconnected). `data`
     *  carries per-event fields: id, title, badgeUrl, measured. QML
     *  dispatches in RAIndicatorBar.qml. Single-signal payload
     *  approach avoids 7-fold boilerplate plumbing through
     *  GameSession → RAService → AppController. */
    void raIndicator(int kind, const QVariantMap& data);
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

    // Memory regions populated by rc_libretro_memory_init from the core's
    // SET_MEMORY_MAPS descriptors (or by retro_get_memory_data fallback).
    // m_regionsInited tracks whether a destroy() is needed on session end.
    rc_libretro_memory_regions_t m_regions{};
    bool m_regionsInited = false;

    // QNetworkAccessManager is not thread-safe; it must be used on its
    // construction thread (main thread). Constructed here on the main thread.
    QNetworkAccessManager m_nam;
};
