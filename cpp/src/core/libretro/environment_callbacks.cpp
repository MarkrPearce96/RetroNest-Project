#include "environment_callbacks.h"
#include "options_store.h"
#include "retro_log.h"
#include <QDebug>
#include <QSet>

static bool handlePixelFormat(EnvironmentContext* ctx, void* data) {
    auto* fmt = static_cast<retro_pixel_format*>(data);
    if (*fmt != RETRO_PIXEL_FORMAT_0RGB1555 &&
        *fmt != RETRO_PIXEL_FORMAT_RGB565 &&
        *fmt != RETRO_PIXEL_FORMAT_XRGB8888) return false;
    ctx->pixelFormat = *fmt;
    return true;
}

static bool handleGetVariable(EnvironmentContext* ctx, void* data) {
    auto* v = static_cast<retro_variable*>(data);
    if (!ctx->options || !v || !v->key) return false;
    ctx->scratchVariableValue = ctx->options->get(QString::fromUtf8(v->key)).toUtf8();
    v->value = ctx->scratchVariableValue.constData();
    return true;
}

static bool handleVariableUpdate(EnvironmentContext* ctx, void* data) {
    auto* out = static_cast<bool*>(data);
    if (!ctx->options || !out) return false;
    *out = ctx->options->consumeDirty();
    return true;
}

static bool handleCoreOptionsV2(EnvironmentContext* ctx, void* data) {
    auto* opts = static_cast<retro_core_options_v2*>(data);
    if (!opts || !opts->definitions) return false;
    ctx->declaredOptions.clear();
    for (const auto* d = opts->definitions; d->key != nullptr; ++d) {
        CoreOption o;
        o.key = QString::fromUtf8(d->key);
        o.label = QString::fromUtf8(d->desc ? d->desc : d->key);
        o.defaultValue = QString::fromUtf8(d->default_value ? d->default_value : "");
        for (int i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[i].value; ++i)
            o.values << QString::fromUtf8(d->values[i].value);
        ctx->declaredOptions.append(o);
    }
    return true;
}

bool environmentDispatch(EnvironmentContext* ctx, unsigned cmd, void* data) {
    if (!ctx) return false;
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return handlePixelFormat(ctx, data);
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            *static_cast<const char**>(data) = ctx->systemDirectory.constData();
            return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *static_cast<const char**>(data) = ctx->saveDirectory.constData();
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE:
            return handleGetVariable(ctx, data);
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            return handleVariableUpdate(ctx, data);
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
            return handleCoreOptionsV2(ctx, data);
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            auto* cb = static_cast<retro_log_callback*>(data);
            if (!cb) return false;
            *cb = retroLogCallback();
            return true;
        }
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
            return true;  // accept and ignore
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;
        default: {
            static QSet<unsigned> warned;
            if (!warned.contains(cmd)) {
                warned.insert(cmd);
                qInfo() << "[libretro/env] unhandled enum" << cmd;
            }
            return false;
        }
    }
}
