#pragma once
#include "libretro.h"
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
    QVector<CoreOption> declaredOptions;  // captured from SET_CORE_OPTIONS_V2

    // Scratch storage so returned const char* buffers stay alive across calls.
    QByteArray scratchVariableValue;
};

/** Returns true if the enum was handled (libretro semantics). */
bool environmentDispatch(EnvironmentContext* ctx, unsigned cmd, void* data);
