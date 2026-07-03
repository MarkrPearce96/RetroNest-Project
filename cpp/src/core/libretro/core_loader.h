#pragma once

#include "libretro.h"
#include <QString>

/**
 * Holds resolved function pointers for one libretro core dylib.
 * All pointers are non-null after a successful open(); a missing required
 * symbol fails the open() with an error message.
 */
struct CoreSymbols {
    unsigned (*retro_api_version)() = nullptr;
    void (*retro_init)() = nullptr;
    void (*retro_deinit)() = nullptr;
    void (*retro_set_environment)(retro_environment_t) = nullptr;
    void (*retro_set_video_refresh)(retro_video_refresh_t) = nullptr;
    void (*retro_set_audio_sample)(retro_audio_sample_t) = nullptr;
    void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = nullptr;
    void (*retro_set_input_poll)(retro_input_poll_t) = nullptr;
    void (*retro_set_input_state)(retro_input_state_t) = nullptr;
    void (*retro_get_system_info)(struct retro_system_info*) = nullptr;
    void (*retro_get_system_av_info)(struct retro_system_av_info*) = nullptr;
    void (*retro_set_controller_port_device)(unsigned, unsigned) = nullptr;
    void (*retro_reset)() = nullptr;
    void (*retro_run)() = nullptr;
    bool (*retro_load_game)(const struct retro_game_info*) = nullptr;
    void (*retro_unload_game)() = nullptr;
    unsigned (*retro_get_region)() = nullptr;
    size_t (*retro_serialize_size)() = nullptr;
    bool (*retro_serialize)(void*, size_t) = nullptr;
    bool (*retro_unserialize)(const void*, size_t) = nullptr;
    void* (*retro_get_memory_data)(unsigned) = nullptr;
    size_t (*retro_get_memory_size)(unsigned) = nullptr;
    // Optional — may stay nullptr after open():
    void (*retro_cheat_reset)() = nullptr;
    void (*retro_cheat_set)(unsigned, bool, const char*) = nullptr;
    // Optional. PCSX2 libretro exports this; mGBA and other cores do
    // not. CoreLoader resolves via resolveOptional, so this stays
    // nullptr when not exported. CoreRuntime checks for null before
    // calling.
    using retronest_set_paused_t = void (*)(bool);
    retronest_set_paused_t retronest_set_paused = nullptr;

    // Optional. PCSX2 libretro exports this so the host can engage the
    // in-VM Turbo limiter — calling retro_run faster doesn't speed up
    // PCSX2 because the EmuThread paces itself internally. Cores
    // without it use the standard speed-multiplier path (works for
    // synchronous cores like mGBA).
    using retronest_set_fast_forward_t = void (*)(bool);
    retronest_set_fast_forward_t retronest_set_fast_forward = nullptr;

    // Optional. PCSX2 exports this after a shutdown-timeout: true means the
    // core detached a wedged VM thread, and the host MUST skip retro_deinit
    // and dlclose (keep the dylib mapped so the detached thread never runs
    // unmapped code) — see the CoreRuntime teardown wedge check.
    using retronest_shutdown_wedged_t = bool (*)();
    retronest_shutdown_wedged_t retronest_shutdown_wedged = nullptr;
};

class CoreLoader {
public:
    CoreLoader() = default;
    ~CoreLoader();
    CoreLoader(const CoreLoader&) = delete;
    CoreLoader& operator=(const CoreLoader&) = delete;

    /** Open dylib at `path` and resolve symbols. On failure, writes a
     *  human-readable message to *err if err != nullptr. */
    bool open(const QString& path, QString* err = nullptr);
    void close();
    bool isOpen() const { return m_handle != nullptr; }
    const CoreSymbols& symbols() const { return m_syms; }
    // Exposed for unit tests that need to dlsym test-only accessors from the
    // loaded dylib (e.g. retronest_test_pause_call_count in fake_libretro_core).
    void* handle() const { return m_handle; }

private:
    void* m_handle = nullptr;
    CoreSymbols m_syms;
};
