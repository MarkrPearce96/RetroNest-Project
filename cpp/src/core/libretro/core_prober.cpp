#include "core_prober.h"
#include "libretro.h"

#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <dlfcn.h>

namespace {

// Capture target for the static env callback. Guarded by g_probeMutex —
// only one probe runs at a time, and the callback fires synchronously
// inside retro_set_environment on the calling thread.
DeclaredOptionsDoc* g_capture = nullptr;

bool probeEnvCallback(unsigned cmd, void* data)
{
    if (!g_capture)
        return false;
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        // Advertise v2 support so cores hand us the rich table (matches
        // RetroNest's real dispatch).
        *static_cast<unsigned*>(data) = 2;
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        populateFromV2(*g_capture, static_cast<const retro_core_options_v2*>(data));
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
        auto* intl = static_cast<const retro_core_options_v2_intl*>(data);
        populateFromV2(*g_capture, intl ? intl->us : nullptr);
        return true;
    }
    default:
        // Cores probe many env commands during set_environment (log iface,
        // dirs, ...). Refusing them is fine — they all have fallbacks, and
        // we never init the core.
        return false;
    }
}

} // namespace

namespace CoreProber {

std::optional<DeclaredOptionsDoc> probe(const QString& coreDylibPath)
{
    static QMutex g_probeMutex;
    // SAFETY: handles are retained for the process lifetime — see header.
    static QHash<QString, void*> g_retainedHandles;

    QMutexLocker lock(&g_probeMutex);

    void* handle = g_retainedHandles.value(coreDylibPath, nullptr);
    if (!handle) {
        handle = dlopen(coreDylibPath.toUtf8().constData(), RTLD_LAZY | RTLD_LOCAL);
        if (!handle) {
            qWarning() << "[CoreProber] dlopen failed for" << coreDylibPath << ":" << dlerror();
            return std::nullopt;
        }
        g_retainedHandles.insert(coreDylibPath, handle);
    }

    auto set_env = reinterpret_cast<void (*)(retro_environment_t)>(
        dlsym(handle, "retro_set_environment"));
    auto get_info = reinterpret_cast<void (*)(retro_system_info*)>(
        dlsym(handle, "retro_get_system_info"));
    if (!set_env || !get_info) {
        qWarning() << "[CoreProber] not a libretro core (missing entry points):" << coreDylibPath;
        return std::nullopt;
    }

    DeclaredOptionsDoc doc;
    g_capture = &doc;
    set_env(&probeEnvCallback);
    g_capture = nullptr;

    if (doc.isEmpty()) {
        qWarning() << "[CoreProber] core declared no options during retro_set_environment:"
                   << coreDylibPath;
        return std::nullopt;
    }

    retro_system_info info{};
    get_info(&info);
    doc.coreLibraryVersion = QString::fromUtf8(info.library_version ? info.library_version : "");
    return doc;
}

} // namespace CoreProber
