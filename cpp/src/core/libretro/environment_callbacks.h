#pragma once
#include "libretro.h"

// RetroNest-private env command IDs (RETRO_ENVIRONMENT_PRIVATE = 0x20000).
//
// 0x20001 — RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW
//           Used by pcsx2_libretro (and future Metal-backed cores) to fetch
//           the NSView* that hosts the core's CAMetalLayer. Output pointer
//           is `void**` written to by the host. Returns false if no native
//           view is registered (mGBA / software cores hit this case).
#define RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW (1 | RETRO_ENVIRONMENT_PRIVATE)

#include <QByteArray>
#include <QString>
#include <QVector>
#include "options_store.h"   // for CoreOption (used in declaredOptions)

class OptionsStore;

struct EnvironmentContext {
    QByteArray systemDirectory;
    QByteArray saveDirectory;
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
