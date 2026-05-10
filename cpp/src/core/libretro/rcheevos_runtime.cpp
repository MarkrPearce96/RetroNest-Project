#include "rcheevos_runtime.h"
#include <rc_client.h>
#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <cstring>

namespace {
// g_active must be visible across threads — rc_client callbacks (login →
// load-game → unlock) fire on whichever thread the corresponding HTTP reply
// finishes on (main thread, where m_nam lives), but beginSession runs on the
// core/worker thread. A thread_local pointer set on the core thread reads as
// nullptr from the main-thread callback, which silently drops the load HTTP
// request and stalls the achievement session forever. Plain static (not
// thread_local) is safe here: only one rcheevos session runs at a time
// (libretro is single-game), and the assignment in beginSession/endSession
// happens-before any cross-thread read in practice (the main-thread read
// only occurs after the core thread has finished issuing the synchronous
// rc_client call that captured the pointer).
//
// g_syms stays thread_local because it's only read from the core thread
// (readMemoryHandler is called from rc_client_do_frame which runs there).
RcheevosRuntime* g_active = nullptr;
thread_local const CoreSymbols* g_syms = nullptr;
} // namespace

// ---------------------------------------------------------------------------
// HTTP handler
//
// Called from the core (worker) thread by rc_client. We hop the actual
// network call to the main thread via QueuedConnection on m_nam, then invoke
// the rc_client callback once the reply finishes (also on main thread —
// rc_client documents its callbacks as safe to call from any thread; it
// queues post-callback work into the next do_frame).
// ---------------------------------------------------------------------------
void RcheevosRuntime::httpHandler(const rc_api_request_t* request,
                                   rc_client_server_callback_t callback,
                                   void* callback_data, rc_client_t* /*client*/) {
    // Capture everything before leaving this stack frame.
    const QString url = QString::fromUtf8(request->url);
    const QByteArray postData = request->post_data
                                ? QByteArray(request->post_data)
                                : QByteArray();
    const QByteArray contentType =
        (request->content_type && *request->content_type)
        ? QByteArray(request->content_type)
        : QByteArray("application/x-www-form-urlencoded");

    // g_active is a plain static (not thread_local) — see the namespace
    // declaration above for why. Reads from any thread see the value set by
    // beginSession on the core thread.
    RcheevosRuntime* self = g_active;
    if (!self) return;  // Session already ended; silently drop.

    // Capture the user-agent on the calling thread before hopping —
    // implicit-shared QByteArray copy is cheap and avoids reading
    // the member from a different thread.
    const QByteArray userAgent = self->m_userAgent;

    // Hop to the main thread where m_nam lives.
    QMetaObject::invokeMethod(self, [self, url, postData, contentType,
                                      userAgent, callback, callback_data]() {
        QUrl qurl(url);
        QNetworkRequest req(qurl);
        req.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
        if (!postData.isEmpty()) {
            req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
        }

        QNetworkReply* reply = postData.isEmpty()
                               ? self->m_nam.get(req)
                               : self->m_nam.post(req, postData);

        QObject::connect(reply, &QNetworkReply::finished, reply,
                         [reply, callback, callback_data]() {
            const QByteArray body = reply->readAll();
            const int httpStatus =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            rc_api_server_response_t response{};
            response.body             = body.constData();
            response.body_length      = static_cast<size_t>(body.size());
            response.http_status_code = (httpStatus > 0) ? httpStatus : 500;

            callback(&response, callback_data);

            reply->deleteLater();
        });
    }, Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Memory reader
//
// Routes through rc_libretro_memory_read, which knows the per-console RA
// memory layout and walks the descriptors captured from SET_MEMORY_MAPS to
// translate RA-internal addresses into host pointers. For GBA this is
// essential — IWRAM (0x03000000) is not contiguous with EWRAM (0x02000000),
// so the previous direct retro_get_memory_data(SYSTEM_RAM) read returned
// zeros for any IWRAM-resident achievement condition.
// ---------------------------------------------------------------------------
uint32_t RcheevosRuntime::readMemoryHandler(uint32_t address, uint8_t* buffer,
                                             uint32_t num_bytes,
                                             rc_client_t* /*client*/) {
    if (!g_active || !buffer) return 0;
    if (!g_active->m_regionsInited) {
        std::memset(buffer, 0, num_bytes);
        return 0;
    }
    return rc_libretro_memory_read(
        &g_active->m_regions, address, buffer, num_bytes);
}

// ---------------------------------------------------------------------------
// Fallback core-memory accessor for rc_libretro_memory_init. Used for
// consoles whose cores don't send SET_MEMORY_MAPS — rc_libretro queries
// retro_get_memory_data(id) for SYSTEM_RAM / SAVE_RAM / VIDEO_RAM / RTC.
// ---------------------------------------------------------------------------
static void rcheevos_get_core_memory_info(uint32_t id,
                                          rc_libretro_core_memory_info_t* info) {
    if (!info) return;
    info->data = nullptr;
    info->size = 0;
    if (!g_syms) return;
    info->data = static_cast<uint8_t*>(g_syms->retro_get_memory_data(id));
    info->size = g_syms->retro_get_memory_size(id);
}

// ---------------------------------------------------------------------------
// Event handler — called from core thread via rc_client_do_frame
// ---------------------------------------------------------------------------
void RcheevosRuntime::eventHandler(const rc_client_event_t* ev, rc_client_t* client) {
    if (!g_active || !ev) return;

    switch (ev->type) {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED: {
        if (!ev->achievement) return;
        // Skip synthetic entries — same canonical signal we use in
        // achievementListVariants (points == 0). Without this filter,
        // rcheevos's "Warning: Unknown Emulator" entry triggers an
        // achievement-unlocked toast at hardcore-on time as if the
        // user had earned a real achievement.
        if (ev->achievement->points == 0) return;
        // RA serves badge images at media.retroachievements.org/Badge/<name>.png
        // (with `_lock` for the locked variant). The unlocked variant is the
        // colored, fully-saturated badge — what we want for the toast.
        char urlBuf[256] = {0};
        rc_client_achievement_get_image_url(ev->achievement,
                                            RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED,
                                            urlBuf, sizeof(urlBuf));
        emit g_active->achievementUnlocked(
            QString::number(ev->achievement->id),
            QString::fromUtf8(ev->achievement->title
                              ? ev->achievement->title : ""),
            QString::fromUtf8(ev->achievement->description
                              ? ev->achievement->description : ""),
            QString::fromUtf8(urlBuf));
        break;
    }

    case RC_CLIENT_EVENT_GAME_COMPLETED: {
        // All achievements for the game (or its core set) earned.
        const rc_client_game_t* game = rc_client_get_game_info(client);
        char imgBuf[256] = {0};
        if (game) rc_client_game_get_image_url(game, imgBuf, sizeof(imgBuf));
        rc_client_user_game_summary_t summary{};
        rc_client_get_user_game_summary(client, &summary);
        const QString title = (game && game->title)
            ? QString::fromUtf8(game->title) : QStringLiteral("Game");
        const QString desc = QStringLiteral("All %1 achievements earned!")
                             .arg(summary.num_core_achievements);
        emit g_active->raInfoToast(QStringLiteral("GAME MASTERED"),
                                    title, desc,
                                    QString::fromUtf8(imgBuf), 8000);
        qInfo().noquote() << "[rcheevos] Game mastered:" << title;
        break;
    }

    case RC_CLIENT_EVENT_RESET: {
        // Fired when hardcore mode is toggled on with a game already
        // loaded — rcheevos demands the emulator reset so any prior
        // save-state contamination is wiped before the hardcore run
        // is allowed to count. We MUST actually reset the core, or
        // server-side validation will void any post-toggle unlocks.
        // eventHandler runs on the core thread (called from inside
        // rc_client_do_frame), so retro_reset is safe to invoke here.
        if (g_syms && g_syms->retro_reset) g_syms->retro_reset();
        emit g_active->raInfoToast(
            QStringLiteral("HARDCORE MODE"),
            QStringLiteral("System reset"),
            QStringLiteral("Hardcore mode enabled — game restarted to validate the run."),
            QString(), 6000);
        qInfo() << "[rcheevos] Hardcore reset (event 14): retro_reset invoked";
        break;
    }

    case RC_CLIENT_EVENT_SERVER_ERROR: {
        // Non-retryable server error (auth, rate limit, malformed
        // payload, etc). Surface to the user — silent failures used
        // to leave the user wondering why an unlock didn't land.
        const QString api = (ev->server_error && ev->server_error->api)
            ? QString::fromUtf8(ev->server_error->api) : QString();
        const QString msg = (ev->server_error && ev->server_error->error_message)
            ? QString::fromUtf8(ev->server_error->error_message)
            : QStringLiteral("(no message)");
        emit g_active->raInfoToast(
            QStringLiteral("RA SERVER ERROR"),
            api.isEmpty() ? QStringLiteral("Submission failed") : api,
            msg, QString(), 8000);
        qWarning().noquote() << "[rcheevos] Server error" << api << ":" << msg;
        break;
    }

    // ── Persistent indicator-bar events ────────────────────────────────
    // All of these flow through the single raIndicator(kind, data) signal
    // so QML can drive a single RAIndicatorBar component without
    // 7-deep signal-forward plumbing.
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW: {
        if (!ev->achievement) break;
        QVariantMap data;
        data["id"] = QString::number(ev->achievement->id);
        data["title"] = QString::fromUtf8(ev->achievement->title
                                          ? ev->achievement->title : "");
        // Locked variant: the user is mid-challenge, hasn't earned it.
        char buf[256] = {0};
        rc_client_achievement_get_image_url(ev->achievement,
                                            RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE,
                                            buf, sizeof(buf));
        data["badgeUrl"] = QString::fromUtf8(buf);
        emit g_active->raIndicator(static_cast<int>(ev->type), data);
        break;
    }
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE: {
        if (!ev->achievement) break;
        QVariantMap data;
        data["id"] = QString::number(ev->achievement->id);
        emit g_active->raIndicator(static_cast<int>(ev->type), data);
        break;
    }
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE: {
        if (!ev->achievement) break;
        QVariantMap data;
        data["id"] = QString::number(ev->achievement->id);
        data["title"] = QString::fromUtf8(ev->achievement->title
                                          ? ev->achievement->title : "");
        data["measured"] = QString::fromUtf8(ev->achievement->measured_progress);
        char buf[256] = {0};
        rc_client_achievement_get_image_url(ev->achievement,
                                            RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE,
                                            buf, sizeof(buf));
        data["badgeUrl"] = QString::fromUtf8(buf);
        emit g_active->raIndicator(static_cast<int>(ev->type), data);
        break;
    }
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE: {
        if (!ev->achievement) break;
        QVariantMap data;
        data["id"] = QString::number(ev->achievement->id);
        emit g_active->raIndicator(static_cast<int>(ev->type), data);
        break;
    }
    case RC_CLIENT_EVENT_DISCONNECTED:
    case RC_CLIENT_EVENT_RECONNECTED: {
        // No payload — QML just toggles the connection-status chip.
        emit g_active->raIndicator(static_cast<int>(ev->type), QVariantMap{});
        qInfo().noquote() << "[rcheevos]"
            << (ev->type == RC_CLIENT_EVENT_DISCONNECTED
                ? "Disconnected — pending unlocks queued"
                : "Reconnected — pending unlocks flushed");
        break;
    }

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Load-game callback (step 2 of the async chain)
// ---------------------------------------------------------------------------
void RcheevosRuntime::loadGameCallback(int result, const char* errorMessage,
                                        rc_client_t* client, void* userdata) {
    auto* self = static_cast<RcheevosRuntime*>(userdata);
    if (result == RC_OK) {
        self->m_inSession = true;
        // Derive counts from the SAME filtered walk the popup uses, so
        // banner ("X / Y earned") and popup header always agree. The
        // raw rc_client_get_user_game_summary numbers can include
        // rcheevos's synthetic "Unknown Emulator" entry (id==0,
        // 0 pts) — see achievementListVariants for why we strip it.
        const rc_client_game_t* game = rc_client_get_game_info(client);
        const QVariantList list = self->achievementListVariants();
        int totalCount = list.size();
        int unlockedCount = 0;
        for (const QVariant& v : list)
            if (v.toMap().value("earned").toBool()) ++unlockedCount;

        // Once-per-session info line. Worth keeping in production: the
        // achievement count + region total are the fastest way to spot
        // when a game hash matched the wrong RA entry, or when memory
        // regions failed to map (count=0 → unlocks can never fire even
        // though the session is technically "active").
        qInfo().nospace()
            << "[rcheevos] Game loaded; achievement session active. "
            << "Title=\"" << (game && game->title ? game->title : "(unknown)")
            << "\" id=" << (game ? game->id : 0)
            << " achievements=" << totalCount
            << " unlocked=" << unlockedCount
            << " regions=" << self->m_regions.count
            << " region_total_bytes=" << self->m_regions.total_size;

        // Game-start banner: tells the user they're playing a tracked
        // game and how much of the achievement set they've already
        // earned. Skipped if the game has no achievements (no toast
        // adds value when there's nothing to track).
        if (totalCount > 0) {
            char gameImg[256] = {0};
            if (game) rc_client_game_get_image_url(game, gameImg, sizeof(gameImg));
            const QString title = (game && game->title)
                ? QString::fromUtf8(game->title) : QStringLiteral("Game");
            const QString desc = QStringLiteral("%1 / %2 achievements earned")
                .arg(unlockedCount)
                .arg(totalCount);
            emit self->raInfoToast(QStringLiteral("ACHIEVEMENTS ACTIVE"),
                                   title, desc,
                                   QString::fromUtf8(gameImg), 5000);
        }
    } else {
        qWarning() << "[rcheevos] rc_client_begin_identify_and_load_game failed:"
                   << (errorMessage ? errorMessage : "(no message)");
    }
}

// ---------------------------------------------------------------------------
// Login callback (step 1 of the async chain)
// ---------------------------------------------------------------------------
void RcheevosRuntime::loginCallback(int result, const char* errorMessage,
                                     rc_client_t* client, void* userdata) {
    auto* self = static_cast<RcheevosRuntime*>(userdata);
    if (result != RC_OK) {
        qWarning() << "[rcheevos] Login failed:"
                   << (errorMessage ? errorMessage : "(no message)");
        return;
    }
    qInfo() << "[rcheevos] Login succeeded; loading game…";
    rc_client_begin_identify_and_load_game(
        client,
        static_cast<uint32_t>(self->m_pendingConsoleId),
        self->m_pendingRomPath.toUtf8().constData(),
        nullptr, 0,
        loadGameCallback, self);
}

// ---------------------------------------------------------------------------
// RcheevosRuntime
// ---------------------------------------------------------------------------

RcheevosRuntime::RcheevosRuntime(QObject* parent) : QObject(parent) {}

RcheevosRuntime::~RcheevosRuntime() { endSession(); }

bool RcheevosRuntime::beginSession(const CoreSymbols& syms,
                                   const QString& romPath,
                                   int raConsoleId,
                                   const QString& username,
                                   const QString& token,
                                   bool hardcore,
                                   bool encore,
                                   const retro_memory_map* memoryMap) {
    if (m_inSession) endSession();
    g_active = this;
    g_syms = &syms;

    // Build the memory-region map BEFORE creating rc_client. The reader is
    // invoked from rc_client internals during do_frame; if regions aren't
    // ready it returns zeros and conditions never match. Even with a null
    // memoryMap, rc_libretro falls back to per-console retro_get_memory_data
    // queries via rcheevos_get_core_memory_info — works for simple cores.
    if (rc_libretro_memory_init(&m_regions, memoryMap,
                                 rcheevos_get_core_memory_info,
                                 static_cast<uint32_t>(raConsoleId))) {
        m_regionsInited = true;
    } else {
        qWarning() << "[rcheevos] rc_libretro_memory_init failed for console"
                   << raConsoleId << "— achievements will not unlock.";
        m_regionsInited = false;
    }

    m_client = rc_client_create(readMemoryHandler, httpHandler);
    if (!m_client) {
        if (m_regionsInited) {
            rc_libretro_memory_destroy(&m_regions);
            m_regionsInited = false;
        }
        g_active = nullptr;
        g_syms = nullptr;
        return false;
    }
    rc_client_set_hardcore_enabled(m_client, hardcore ? 1 : 0);
    rc_client_set_encore_mode_enabled(m_client, encore ? 1 : 0);
    rc_client_set_event_handler(m_client, eventHandler);

    // Build the User-Agent string once per session. RA's server uses
    // this to recognize the frontend; an unrecognized UA triggers the
    // synthetic "Unknown Emulator" warning achievement and (post-
    // approval) is what flips hardcore unlocks from "rejected" to
    // "validated". The rcheevos clause version-stamps the rcheevos
    // library so RA's server knows which protocol version we speak.
    //
    // Pull the app version from QCoreApplication so version bumps in
    // the binary roll forward into RA's request log without us having
    // to remember to edit a hardcoded string. Falls back to "unknown"
    // if main.cpp didn't call setApplicationVersion (defensive — it
    // should always be set in real runs).
    char uaClause[64] = {0};
    rc_client_get_user_agent_clause(m_client, uaClause, sizeof(uaClause));
    const QByteArray appName = QCoreApplication::applicationName().isEmpty()
        ? QByteArrayLiteral("RetroNest")
        : QCoreApplication::applicationName().toUtf8();
    const QByteArray appVer = QCoreApplication::applicationVersion().isEmpty()
        ? QByteArrayLiteral("unknown")
        : QCoreApplication::applicationVersion().toUtf8();
    m_userAgent = appName + "/" + appVer + " " + QByteArray(uaClause);

    if (token.isEmpty() || username.isEmpty()) {
        emit loginRequired();
        // Continue without RA — frame ticks become no-ops until
        // m_inSession is set true by the load-game callback.
        return false;
    }

    // Store pending values for the chained callbacks.
    m_pendingRomPath   = romPath;
    m_pendingConsoleId = raConsoleId;

    rc_client_begin_login_with_token(m_client,
        username.toUtf8().constData(),
        token.toUtf8().constData(),
        loginCallback, this);

    // m_inSession stays false until loadGameCallback succeeds.
    return true;
}

void RcheevosRuntime::endSession() {
    if (m_client) {
        rc_client_destroy(m_client);
        m_client = nullptr;
    }
    if (m_regionsInited) {
        rc_libretro_memory_destroy(&m_regions);
        m_regionsInited = false;
    }
    g_active = nullptr;
    g_syms = nullptr;
    m_inSession = false;
}

void RcheevosRuntime::frame() {
    if (!m_enabled) return;
    if (m_client && m_inSession) rc_client_do_frame(m_client);
}

void RcheevosRuntime::setHardcore(bool on) {
    if (m_client)
        rc_client_set_hardcore_enabled(m_client, on ? 1 : 0);
}

void RcheevosRuntime::setEncore(bool on) {
    if (m_client)
        rc_client_set_encore_mode_enabled(m_client, on ? 1 : 0);
}

QVariantList RcheevosRuntime::achievementListVariants() {
    QVariantList result;
    if (!m_client) return result;
    // CORE category only, matching what the RA web API's
    // GetGameInfoAndUserProgress returns. This is what the popup
    // counter "X / Y earned" should reflect.
    rc_client_achievement_list_t* list = rc_client_create_achievement_list(
        m_client,
        RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE,
        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list) return result;

    for (uint32_t b = 0; b < list->num_buckets; ++b) {
        const rc_client_achievement_bucket_t& bucket = list->buckets[b];
        for (uint32_t i = 0; i < bucket.num_achievements; ++i) {
            const rc_client_achievement_t* a = bucket.achievements[i];
            if (!a) continue;
            // Skip rcheevos-synthetic entries. rc_client injects a
            // pseudo-achievement (e.g. id=101000001 "Warning: Unknown
            // Emulator", state=UNLOCKED, points=0) when the host
            // frontend isn't on RA's approved-emulator list — its
            // intent is to flag that hardcore unlocks won't validate
            // server-side. The web API's GetGameInfoAndUserProgress
            // endpoint doesn't return this entry, so it artificially
            // inflated our local counts by 1 in both numerator and
            // denominator. Real RA achievements always carry positive
            // point values; synthetic ones use 0. That's the canonical
            // distinguishing field.
            if (a->points == 0) continue;
            QVariantMap m;
            m["id"] = QString::number(a->id);
            m["title"] = QString::fromUtf8(a->title ? a->title : "");
            m["description"] = QString::fromUtf8(a->description ? a->description : "");
            m["points"] = static_cast<int>(a->points);
            const bool earned =
                (a->state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
            m["earned"] = earned;
            // RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE = 1; matches the
            // RA web API's "type" int 1 so the popup's filter logic
            // is identical for both data paths.
            m["missable"] =
                (a->type == RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE);
            // Coloured badge for earned, locked variant for unlocked.
            char buf[256] = {0};
            rc_client_achievement_get_image_url(
                a,
                earned ? RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED
                       : RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE,
                buf, sizeof(buf));
            m["badgeUrl"] = QString::fromUtf8(buf);
            // Optional progress text — empty for non-trackable
            // achievements; the popup just doesn't render it.
            m["measured"] = QString::fromUtf8(a->measured_progress);
            result.append(m);
        }
    }
    rc_client_destroy_achievement_list(list);
    return result;
}

void RcheevosRuntime::setEnabled(bool on) {
    m_enabled = on;
}
