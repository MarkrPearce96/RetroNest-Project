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

private:
    void* m_handle = nullptr;
    CoreSymbols m_syms;
};
