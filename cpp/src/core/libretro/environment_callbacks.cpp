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

// Weak stub; overridden by core_runtime.cpp when linking full app.
// This allows test_environment_callbacks to link without core_runtime dependencies.
extern "C" void* coreRuntimeGetActiveNSView(void* runtime_opaque) __attribute__((weak));
void* coreRuntimeGetActiveNSView(void* runtime_opaque) {
    (void)runtime_opaque;  // unused in stub
    return nullptr;
}

// Weak stub for the rumble bridge — strong override in core_runtime.cpp.
extern "C" bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                          unsigned port,
                                          unsigned effect,
                                          uint16_t strength) __attribute__((weak));
bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                unsigned port,
                                unsigned effect,
                                uint16_t strength) {
    (void)runtime_opaque; (void)port; (void)effect; (void)strength;
    return false;
}

// Static thunk the core stores via retro_rumble_interface.set_rumble_state.
// libretro doesn't pass our context pointer to the thunk, so we route
// through a file-scope pointer stashed at GET_RUMBLE_INTERFACE time.
namespace {
EnvironmentContext* g_rumbleCtx = nullptr;
bool rumbleThunk(unsigned port, retro_rumble_effect effect, uint16_t strength) {
    if (!g_rumbleCtx || !g_rumbleCtx->runtime) return false;
    return coreRuntimeSetRumbleMotor(g_rumbleCtx->runtime, port,
                                     static_cast<unsigned>(effect), strength);
}
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
        case RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW: {
            if (!data) {
                qWarning("[libretro/env] GET_MACOS_NSVIEW: data=null");
                return false;
            }
            if (!ctx->runtime) {
                qWarning("[libretro/env] GET_MACOS_NSVIEW: ctx->runtime=null");
                return false;
            }
            void* ns_view = coreRuntimeGetActiveNSView(ctx->runtime);
            if (!ns_view) {
                qWarning("[libretro/env] GET_MACOS_NSVIEW: activeNSView returned null");
                return false;
            }
            qInfo("[libretro/env] GET_MACOS_NSVIEW: returning ns_view=%p", ns_view);
            *reinterpret_cast<void**>(data) = ns_view;
            return true;
        }
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
        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            auto* iface = static_cast<retro_rumble_interface*>(data);
            if (!iface) return false;
            g_rumbleCtx = ctx;             // stash for the thunk
            iface->set_rumble_state = &rumbleThunk;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
            return true;  // accept and ignore
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;
        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS: {
            // The core declares its memory layout; rcheevos uses this to
            // translate from RA's normalized address space to actual host
            // memory pointers. Without this, achievement conditions can't
            // resolve any address outside SYSTEM_RAM and never trigger.
            //
            // The libretro spec requires the FRONTEND to retain its own
            // copy of the descriptor array and the addrspace strings — the
            // core may free both after the call returns. Reserve before
            // appending so QByteArray::constData() pointers stay stable
            // when we reference them from the descriptor copies.
            const auto* mm = static_cast<const retro_memory_map*>(data);
            if (!mm || !mm->descriptors) return false;
            ctx->memoryDescriptors.clear();
            ctx->memoryAddrspaces.clear();
            ctx->memoryDescriptors.reserve(static_cast<int>(mm->num_descriptors));
            ctx->memoryAddrspaces.reserve(static_cast<int>(mm->num_descriptors));
            for (unsigned i = 0; i < mm->num_descriptors; ++i) {
                retro_memory_descriptor d = mm->descriptors[i];
                ctx->memoryAddrspaces.append(
                    d.addrspace ? QByteArray(d.addrspace) : QByteArray());
                d.addrspace = ctx->memoryAddrspaces.last().constData();
                ctx->memoryDescriptors.append(d);
            }
            ctx->memoryMap.descriptors = ctx->memoryDescriptors.data();
            ctx->memoryMap.num_descriptors =
                static_cast<unsigned>(ctx->memoryDescriptors.size());
            ctx->memoryMapSet = true;
            qInfo() << "[libretro/env] SET_MEMORY_MAPS captured"
                    << ctx->memoryDescriptors.size() << "descriptors";
            return true;
        }
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
