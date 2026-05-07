#include "rcheevos_runtime.h"
#include <rc_client.h>
#include <QDebug>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <cstring>

namespace {
thread_local RcheevosRuntime* g_active = nullptr;
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

    // g_active is thread_local on the CORE thread — grab the pointer now.
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
// ---------------------------------------------------------------------------
uint32_t RcheevosRuntime::readMemoryHandler(uint32_t address, uint8_t* buffer,
                                             uint32_t num_bytes,
                                             rc_client_t* /*client*/) {
    if (!g_syms || !buffer) return 0;
    void* ram = g_syms->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    size_t ramSize = g_syms->retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    if (!ram || address + num_bytes > ramSize) {
        std::memset(buffer, 0, num_bytes);
        return 0;
    }
    std::memcpy(buffer, static_cast<uint8_t*>(ram) + address, num_bytes);
    return num_bytes;
}

// ---------------------------------------------------------------------------
// Event handler — called from core thread via rc_client_do_frame
// ---------------------------------------------------------------------------
void RcheevosRuntime::eventHandler(const rc_client_event_t* ev, rc_client_t* /*client*/) {
    if (!g_active || !ev) return;
    if (ev->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED && ev->achievement) {
        emit g_active->achievementUnlocked(
            QString::number(ev->achievement->id),
            QString::fromUtf8(ev->achievement->title
                              ? ev->achievement->title : ""),
            QString::fromUtf8(ev->achievement->description
                              ? ev->achievement->description : ""));
    }
}

// ---------------------------------------------------------------------------
// Load-game callback (step 2 of the async chain)
// ---------------------------------------------------------------------------
void RcheevosRuntime::loadGameCallback(int result, const char* errorMessage,
                                        rc_client_t* /*client*/, void* userdata) {
    auto* self = static_cast<RcheevosRuntime*>(userdata);
    if (result == RC_OK) {
        self->m_inSession = true;
        qInfo() << "[rcheevos] Game loaded; achievement session active.";
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
                                   bool hardcore) {
    if (m_inSession) endSession();
    g_active = this;
    g_syms = &syms;

    m_client = rc_client_create(readMemoryHandler, httpHandler);
    if (!m_client) {
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
