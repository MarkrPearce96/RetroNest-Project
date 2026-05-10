#include "rcheevos_runtime.h"
#include <rc_client.h>
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

    // Hop to the main thread where m_nam lives.
    QMetaObject::invokeMethod(self, [self, url, postData, contentType,
                                      callback, callback_data]() {
        QUrl qurl(url);
        QNetworkRequest req(qurl);
        req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/0.1");
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
void RcheevosRuntime::eventHandler(const rc_client_event_t* ev, rc_client_t* /*client*/) {
    if (!g_active || !ev) return;

    if (ev->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED && ev->achievement) {
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
        // Diagnostics: log the identified game's title and how many
        // achievements + memory regions are live. Zero-count on either
        // side means unlocks can never fire even though the session is
        // technically "active".
        const rc_client_game_t* game = rc_client_get_game_info(client);
        rc_client_user_game_summary_t summary{};
        rc_client_get_user_game_summary(client, &summary);
        qInfo().nospace()
            << "[rcheevos] Game loaded; achievement session active. "
            << "Title=\"" << (game && game->title ? game->title : "(unknown)")
            << "\" id=" << (game ? game->id : 0)
            << " achievements=" << summary.num_core_achievements
            << " unlocked=" << summary.num_unlocked_achievements
            << " regions=" << self->m_regions.count
            << " region_total_bytes=" << self->m_regions.total_size;
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
    rc_client_set_event_handler(m_client, eventHandler);

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

void RcheevosRuntime::setEnabled(bool on) {
    m_enabled = on;
}
