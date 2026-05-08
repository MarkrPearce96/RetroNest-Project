#include "libretro_adapter.h"
#include "core/paths.h"
#include "core/ini_file.h"
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>

QString LibretroAdapter::coreDylibPath(const EmulatorManifest& manifest) {
    return Paths::emulatorsDir("libretro") + "/cores/" + manifest.core_dylib;
}

QString LibretroAdapter::optionsJsonPath() const {
    return Paths::emulatorsDir("libretro") + "/" + coreId() + "/options.json";
}

QString LibretroAdapter::controlsIniPath() const {
    return Paths::emulatorsDir("libretro") + "/" + coreId() + "/controls.ini";
}

QString LibretroAdapter::controllerBindingsConfigFilePath() const {
    return controlsIniPath();
}

QString LibretroAdapter::controllerBindingsSection(int port) const {
    return QString("Pad%1").arg(port);
}

QString LibretroAdapter::controllerBindingsConfigFilePath(const QString& /*typeId*/) const {
    return controllerBindingsConfigFilePath();
}

QString LibretroAdapter::controllerBindingsSection(int port, const QString& /*typeId*/) const {
    return controllerBindingsSection(port);
}

QString LibretroAdapter::frontendJsonPath() const {
    return Paths::emulatorsDir("libretro") + "/" + coreId() + "/frontend.json";
}

bool LibretroAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                   const QString& /*biosPath*/,
                                   const QString& savesPath) {
    QDir().mkpath(savesPath);
    QDir().mkpath(QFileInfo(optionsJsonPath()).absolutePath());

    // Seed controls.ini with default RetroPad bindings if it doesn't exist yet.
    // Users can override these via the Controller mapping page; we never overwrite
    // an existing file so user edits are preserved across ensureConfig calls.
    const QString iniPath = controlsIniPath();
    if (!QFileInfo::exists(iniPath)) {
        // Build defaults from controllerBindingDefsForType. The canonical SDL element
        // names here fold the old hardcoded X↔Y swap: libretro RetroPad A=south,
        // B=east, X=west, Y=north — SDL physical buttons map straight through.
        const QMap<QString, QString> defaults = {
            { "A",      "SDL-0/FaceSouth"   },
            { "B",      "SDL-0/FaceEast"    },
            { "X",      "SDL-0/FaceWest"    },
            { "Y",      "SDL-0/FaceNorth"   },
            { "L",      "SDL-0/LeftShoulder" },
            { "R",      "SDL-0/RightShoulder"},
            { "Select", "SDL-0/Back"        },
            { "Start",  "SDL-0/Start"       },
            { "Up",     "SDL-0/DPadUp"      },
            { "Down",   "SDL-0/DPadDown"    },
            { "Left",   "SDL-0/DPadLeft"    },
            { "Right",  "SDL-0/DPadRight"   },
        };
        const QString section = controllerBindingsSection(1);
        IniFile ini;
        // Walk binding defs in declared order so keys appear in a stable order.
        for (const auto& def : controllerBindingDefsForType({})) {
            const auto it = defaults.constFind(def.key);
            if (it != defaults.constEnd())
                ini.setValue(section, def.key, it.value());
        }
        if (!ini.save(iniPath))
            qWarning() << "[LibretroAdapter] Failed to write default controls.ini to" << iniPath;
        else
            qInfo() << "[LibretroAdapter] Wrote default controls.ini to" << iniPath;
    }

    return true;
}

QString LibretroAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                           const QString& /*installPath*/) {
    return coreDylibPath(manifest);
}

bool LibretroAdapter::isInstalled(const EmulatorManifest& manifest) {
    return QFileInfo::exists(coreDylibPath(manifest));
}

EmulatorAdapter::DirectDownloadInfo
LibretroAdapter::resolveDirectDownload(const EmulatorManifest& manifest) const {
    DirectDownloadInfo info;
    if (manifest.core_buildbot_path.isEmpty()) return info;
    const QString arch =
#if defined(Q_PROCESSOR_ARM_64)
        "arm64";
#else
        "x86_64";
#endif
    const QString url = QString("https://buildbot.libretro.com/nightly/apple/osx/%1/latest/%2")
                        .arg(arch, manifest.core_buildbot_path);

    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    auto* reply = nam.head(req);
    QEventLoop loop;
    QTimer t; t.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    t.start(8000);
    loop.exec();

    QString lastMod = reply->rawHeader("Last-Modified");
    reply->deleteLater();
    if (lastMod.isEmpty()) lastMod = "unknown";
    info.version = "nightly-" + lastMod.left(16);
    info.publishedAt = lastMod;
    info.assetName = manifest.core_buildbot_path;
    info.downloadUrl = url;
    return info;
}

QString LibretroAdapter::findResumeFile(const QString& /*serial*/) const {
    // Concrete adapters override; libretro resume uses ROM-base-name + ".resume"
    // and is resolved at start time via the StartConfig.resumeStatePath.
    return {};
}

OptionsStore* LibretroAdapter::libretroOptionsStore() {
    return m_runtime ? &m_runtime->options() : nullptr;
}

FrontendSettingsStore* LibretroAdapter::frontendSettingsStore() {
    if (!m_frontendSettings) {
        m_frontendSettings = std::make_unique<FrontendSettingsStore>();
        m_frontendSettings->load(frontendJsonPath(), frontendSettingDefaults());
    }
    return m_frontendSettings.get();
}

void LibretroAdapter::prepareRuntime() {
    if (!m_runtime) m_runtime = std::make_unique<CoreRuntime>();
}

void LibretroAdapter::releaseRuntime() {
    m_runtime.reset();
}

void LibretroAdapter::patchRetroAchievements(const QString& /*username*/,
                                              const QString& /*token*/,
                                              bool enabled, bool hardcore,
                                              bool /*notifications*/, bool /*sounds*/) {
    // rcheevos v12 doesn't expose notification / sound-effect knobs separately
    // from the client; those prefs only affect the standalone emulator UIs.
    if (m_runtime) {
        m_runtime->rcheevos().setEnabled(enabled);
        m_runtime->rcheevos().setHardcore(hardcore);
    }
}
