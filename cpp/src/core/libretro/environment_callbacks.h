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
