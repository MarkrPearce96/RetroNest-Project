#include "core_loader.h"
#include <dlfcn.h>
#include <QDebug>

namespace {
template <typename FN>
bool resolveRequired(void* h, const char* name, FN& out, QString* err) {
    out = reinterpret_cast<FN>(dlsym(h, name));
    if (out) return true;
    if (err) *err = QString("missing required symbol: %1").arg(name);
    return false;
}
template <typename FN>
void resolveOptional(void* h, const char* name, FN& out) {
    out = reinterpret_cast<FN>(dlsym(h, name));
}
}

CoreLoader::~CoreLoader() { close(); }

bool CoreLoader::open(const QString& path, QString* err) {
    if (m_handle) close();
    m_handle = dlopen(path.toUtf8().constData(), RTLD_NOW | RTLD_LOCAL);
    if (!m_handle) {
        if (err) *err = QString("dlopen(%1) failed: %2").arg(path, dlerror());
        return false;
    }
    bool ok = true;
    ok &= resolveRequired(m_handle, "retro_api_version", m_syms.retro_api_version, err);
    ok &= resolveRequired(m_handle, "retro_init", m_syms.retro_init, err);
    ok &= resolveRequired(m_handle, "retro_deinit", m_syms.retro_deinit, err);
    ok &= resolveRequired(m_handle, "retro_set_environment", m_syms.retro_set_environment, err);
    ok &= resolveRequired(m_handle, "retro_set_video_refresh", m_syms.retro_set_video_refresh, err);
    ok &= resolveRequired(m_handle, "retro_set_audio_sample", m_syms.retro_set_audio_sample, err);
    ok &= resolveRequired(m_handle, "retro_set_audio_sample_batch", m_syms.retro_set_audio_sample_batch, err);
    ok &= resolveRequired(m_handle, "retro_set_input_poll", m_syms.retro_set_input_poll, err);
    ok &= resolveRequired(m_handle, "retro_set_input_state", m_syms.retro_set_input_state, err);
    ok &= resolveRequired(m_handle, "retro_get_system_info", m_syms.retro_get_system_info, err);
    ok &= resolveRequired(m_handle, "retro_get_system_av_info", m_syms.retro_get_system_av_info, err);
    ok &= resolveRequired(m_handle, "retro_set_controller_port_device", m_syms.retro_set_controller_port_device, err);
    ok &= resolveRequired(m_handle, "retro_reset", m_syms.retro_reset, err);
    ok &= resolveRequired(m_handle, "retro_run", m_syms.retro_run, err);
    ok &= resolveRequired(m_handle, "retro_load_game", m_syms.retro_load_game, err);
    ok &= resolveRequired(m_handle, "retro_unload_game", m_syms.retro_unload_game, err);
    ok &= resolveRequired(m_handle, "retro_get_region", m_syms.retro_get_region, err);
    ok &= resolveRequired(m_handle, "retro_serialize_size", m_syms.retro_serialize_size, err);
    ok &= resolveRequired(m_handle, "retro_serialize", m_syms.retro_serialize, err);
    ok &= resolveRequired(m_handle, "retro_unserialize", m_syms.retro_unserialize, err);
    ok &= resolveRequired(m_handle, "retro_get_memory_data", m_syms.retro_get_memory_data, err);
    ok &= resolveRequired(m_handle, "retro_get_memory_size", m_syms.retro_get_memory_size, err);
    resolveOptional(m_handle, "retro_cheat_reset", m_syms.retro_cheat_reset);
    resolveOptional(m_handle, "retro_cheat_set", m_syms.retro_cheat_set);
    if (!ok) { close(); return false; }
    return true;
}

void CoreLoader::close() {
    if (m_handle) {
        dlclose(m_handle);
        m_handle = nullptr;
        m_syms = {};
    }
}
