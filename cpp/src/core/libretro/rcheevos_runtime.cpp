#include "rcheevos_runtime.h"
#include <rc_client.h>
#include <QDebug>
#include <cstring>

namespace {
thread_local RcheevosRuntime* g_active = nullptr;
thread_local const CoreSymbols* g_syms = nullptr;

// Stub HTTP handler — real implementation is a follow-up task.
// TODO(rcheevos-http): post to RA web API via QNetworkAccessManager
//   blocking call (we're on the core thread, so blocking is acceptable
//   for the per-frame budget; or post via QMetaObject::invokeMethod to
//   main thread and wait on a std::future).
static void httpHandler(const rc_api_request_t* /*request*/,
                        rc_client_server_callback_t /*callback*/,
                        void* /*callback_data*/, rc_client_t* /*client*/) {
    // No-op: until we wire real HTTP, RA fetches silently fail and the
    // client runs in a degenerate "logged in but no achievements" state.
}

static uint32_t readMemoryHandler(uint32_t address, uint8_t* buffer,
                                  uint32_t num_bytes, rc_client_t* /*client*/) {
    if (!g_syms || !buffer) return 0;
    // Main system RAM region: RETRO_MEMORY_SYSTEM_RAM (0)
    void* ram = g_syms->retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    size_t ramSize = g_syms->retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    if (!ram || address + num_bytes > ramSize) {
        std::memset(buffer, 0, num_bytes);
        return 0;
    }
    std::memcpy(buffer, static_cast<uint8_t*>(ram) + address, num_bytes);
    return num_bytes;
}

static void eventHandler(const rc_client_event_t* ev, rc_client_t* /*client*/) {
    if (!g_active || !ev) return;
    if (ev->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED && ev->achievement) {
        emit g_active->achievementUnlocked(
            QString::number(ev->achievement->id),
            QString::fromUtf8(ev->achievement->title ? ev->achievement->title : ""),
            QString::fromUtf8(ev->achievement->description ? ev->achievement->description : ""));
    }
}
} // namespace

RcheevosRuntime::RcheevosRuntime(QObject* parent) : QObject(parent) {}

RcheevosRuntime::~RcheevosRuntime() { endSession(); }

bool RcheevosRuntime::beginSession(const CoreSymbols& syms,
                                   const QString& /*romPath*/,
                                   int /*raConsoleId*/,
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

    if (token.isEmpty()) {
        emit loginRequired();
        // Continue without RA — frame ticks become harmless no-ops.
        // m_inSession stays false so frame() skips rc_client_do_frame.
        return false;
    }

    // TODO(rcheevos-http): rc_client_begin_login_with_token(...) +
    //   rc_client_begin_identify_and_load_game(...) once HTTP handler
    //   does real work. Right now login posts go to the no-op httpHandler
    //   and never complete.

    m_inSession = true;
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
    if (m_client && m_inSession) rc_client_do_frame(m_client);
}
