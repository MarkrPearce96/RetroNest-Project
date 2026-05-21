#pragma once
#include "libretro.h"
#include <cstdint>

// RetroNest-private env command IDs (RETRO_ENVIRONMENT_PRIVATE = 0x20000).
//
// 0x20001 — RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW
//           Used by pcsx2_libretro (and future Metal-backed cores) to fetch
//           the NSView* that hosts the core's CAMetalLayer. Output pointer
//           is `void**` written to by the host. Returns false if no native
//           view is registered (mGBA / software cores hit this case).
#define RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW (1 | RETRO_ENVIRONMENT_PRIVATE)

// 0x20002 — RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH
//           Used by pcsx2_libretro to receive a resume-state path from
//           RetroNest BEFORE the core's VM init runs, so PCSX2 can load
//           the state via VMBootParameters::save_state — i.e., after
//           full BIOS init / ELF discovery, which is the only ordering
//           that produces a runnable VM for cold-resume on launch.
//           Output is a `const char**` written with a UTF-8 path. The
//           env handler sets EnvironmentContext::bootStatePathConsumed
//           on read so CoreRuntime knows to skip the legacy post-load
//           retro_unserialize block; the QByteArray storage stays alive
//           for the env_cb call's synchronous duration (clearing it
//           here would dangle the caller's pointer).
//           Returns false if no path is set (mGBA / fresh-boot cases).
#define RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH (2 | RETRO_ENVIRONMENT_PRIVATE)

// Path-override env queries. Each returns the user's override (set via
// the Paths settings UI; stored in PathOverridesStore) for one PCSX2-
// owned folder. Same shape as GET_BOOT_STATE_PATH (0x20002): data is
// const char**, returns true with *data set when override exists,
// false otherwise. The libretro core falls back to its save_dir-based
// default when false comes back, so non-RetroNest hosts and mGBA
// (which doesn't query these) keep working unchanged.

// 0x20003 — RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR
#define RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR (3 | RETRO_ENVIRONMENT_PRIVATE)

// 0x20004 — RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR
#define RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR (4 | RETRO_ENVIRONMENT_PRIVATE)

#include <QByteArray>
#include <QString>
#include <QVector>
#include "options_store.h"   // for CoreOption (used in declaredOptions)

class OptionsStore;

struct EnvironmentContext {
    QByteArray systemDirectory;
    QByteArray saveDirectory;
    // SP6.5 Task 4.5: resume-state path the libretro core consumes
    // synchronously during retro_load_game via
    // RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH. Set by CoreRuntime::runLoop
    // before retro_load_game; consumed via bootStatePathConsumed flag
    // (see below) so the storage stays alive for the duration of the
    // env_cb call — calling clear() here would free the buffer that
    // constData() pointed at and dangle the caller's path pointer
    // (QByteArray refcount goes to 0 because toUtf8() returns an
    // unshared QByteArray).
    QByteArray bootStatePath;

    // True once the core has called GET_BOOT_STATE_PATH and we've handed
    // off the path. CoreRuntime checks this AFTER retro_load_game returns
    // to decide whether the core consumed the path (skip the legacy
    // retro_unserialize fallback) or not (mGBA: still run the fallback).
    bool bootStatePathConsumed = false;

    retro_pixel_format pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;
    OptionsStore* options = nullptr;
    void* runtime = nullptr;  // CoreRuntime* (opaque to avoid circular includes)
    QVector<CoreOption> declaredOptions;  // captured from SET_CORE_OPTIONS_V2

    // Scratch storage so returned const char* buffers stay alive across calls.
    QByteArray scratchVariableValue;

    // Captured from RETRO_ENVIRONMENT_SET_MEMORY_MAPS. The libretro spec
    // requires the frontend to retain its own copy of the descriptors and
    // their addrspace strings — the core may free them after the call.
    // Used by RcheevosRuntime to translate RA memory addresses through
    // rc_libretro_memory_init.
    QVector<retro_memory_descriptor> memoryDescriptors;
    QVector<QByteArray> memoryAddrspaces;   // backing storage for descriptor.addrspace
    retro_memory_map memoryMap{};            // points into memoryDescriptors
    bool memoryMapSet = false;

    // Task #7 stub: captured from RETRO_ENVIRONMENT_SET_HW_RENDER. The dispatch
    // currently returns false (we can't yet grant a context), but stashing the
    // callback lets us diagnose what cores are asking for and gives the real
    // implementation a place to land. context_type values of interest:
    // RETRO_HW_CONTEXT_OPENGL_CORE (PPSSPP), VULKAN (future), D3D11 (Windows).
    retro_hw_render_callback hwRender{};
    bool hwRenderRequested = false;

    // Task #7 stub: RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT is a hint that
    // the core will spawn threads needing GL access. PPSSPP libretro does NOT
    // call this (see reference_retroarch_ppsspp_pattern.md) but we accept it
    // for any future GL core that does. RetroArch's user-config default for
    // video_shared_context is true regardless of this flag, so the real
    // implementation will create shared contexts unconditionally.
    bool hwSharedContextRequested = false;
};

/** Returns true if the enum was handled (libretro semantics). */
bool environmentDispatch(EnvironmentContext* ctx, unsigned cmd, void* data);

/**
 * Bridge function to get the NSView from a CoreRuntime.
 * Implemented in core_runtime.cpp; used by environmentDispatch for
 * RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW.
 * (Weak stub in environment_callbacks.cpp is overridden by strong implementation in core_runtime.cpp)
 */
extern "C" void* coreRuntimeGetActiveNSView(void* runtime_opaque);

/**
 * Bridge function to fire rumble on the SdlInputManager that the active
 * CoreRuntime is wired to. Implemented in core_runtime.cpp; used by
 * environment_callbacks for the RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE
 * thunk. Weak stub in environment_callbacks.cpp lets test_environment_callbacks
 * link without dragging in core_runtime.
 *
 * Returns true if the call reached a live controller, false otherwise.
 * The libretro set_rumble_state contract has no failure semantics, so
 * callers should ignore the return value in production.
 */
extern "C" bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                          unsigned port,
                                          unsigned effect,
                                          uint16_t strength);

/**
 * Bridge function used by RETRO_ENVIRONMENT_SET_MESSAGE / SET_MESSAGE_EXT to
 * surface a core-generated OSD message via CoreRuntime::coreMessage. The env
 * thunk is invoked on the libretro worker thread; the runtime emits the
 * Qt signal across the thread boundary (auto-routes to queued connection).
 *
 * `text` is UTF-8, owned by the caller for the duration of this call.
 * `durationMs` is the requested display duration (0 = frontend default).
 *
 * Implemented in core_runtime.cpp; weak stub in environment_callbacks.cpp
 * keeps test_environment_callbacks linkable without core_runtime.
 */
extern "C" void coreRuntimeEmitMessage(void* runtime_opaque,
                                       const char* text,
                                       int durationMs);
