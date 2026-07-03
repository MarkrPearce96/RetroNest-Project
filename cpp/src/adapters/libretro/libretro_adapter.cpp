#include "libretro_adapter.h"
#include "core/paths.h"
#include "core/ini_file.h"
#include "core/setting_def.h"
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

    // Seed controls.ini with default RetroPad bindings, backfilling any default key
    // that is MISSING from an existing file (e.g. L2/R2, which earlier versions never
    // seeded — leaving DualSense triggers dead for the digital-button JOYPAD read).
    // We only ADD absent keys; existing values are never overwritten, so user edits via
    // the Controller mapping page are preserved.
    //
    // The canonical SDL element names here fold the old hardcoded X↔Y swap: libretro
    // RetroPad A=south, B=east, X=west, Y=north — SDL physical buttons map straight
    // through. L2/R2 bind the trigger axes (+LeftTrigger/+RightTrigger); the SDL input
    // layer thresholds those to digital L2/R2 presses (sdl_input_manager.cpp).
    const QString iniPath = controlsIniPath();
    {
        const QMap<QString, QString> defaults = {
            { "A",      "SDL-0/FaceSouth"   },
            { "B",      "SDL-0/FaceEast"    },
            { "X",      "SDL-0/FaceWest"    },
            { "Y",      "SDL-0/FaceNorth"   },
            { "L",      "SDL-0/LeftShoulder" },
            { "R",      "SDL-0/RightShoulder"},
            { "L2",     "SDL-0/+LeftTrigger"  },
            { "R2",     "SDL-0/+RightTrigger" },
            { "Select", "SDL-0/Back"        },
            { "Start",  "SDL-0/Start"       },
            { "Up",     "SDL-0/DPadUp"      },
            { "Down",   "SDL-0/DPadDown"    },
            { "Left",   "SDL-0/DPadLeft"    },
            { "Right",  "SDL-0/DPadRight"   },
        };
        const QString section = controllerBindingsSection(1);
        IniFile ini;
        const bool existed = QFileInfo::exists(iniPath);
        if (existed)
            ini.load(iniPath);
        bool changed = false;
        // Walk binding defs in declared order so new keys append in a stable order.
        for (const auto& def : controllerBindingDefsForType({})) {
            const auto it = defaults.constFind(def.key);
            if (it == defaults.constEnd())
                continue;
            if (!ini.containsKey(section, def.key)) {
                ini.setValue(section, def.key, it.value());
                changed = true;
            }
        }
        if (changed) {
            if (!ini.save(iniPath))
                qWarning() << "[LibretroAdapter] Failed to write controls.ini to" << iniPath;
            else
                qInfo() << "[LibretroAdapter]" << (existed ? "Backfilled missing bindings in" : "Wrote default")
                        << "controls.ini at" << iniPath;
        }
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
    // No shipped manifest sets core_buildbot_path anymore (mgba's was
    // removed in packet 6: the buildbot serves single-arch nightlies picked
    // by the frontend's compile arch, so an in-app "update" would downgrade
    // the locally built universal core). Kept as generic infrastructure for
    // any future manifest that opts into buildbot distribution.
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

QVector<EmulatorAdapter::AssetMatchRule> LibretroAdapter::assetMatchRules() const {
    // One macOS asset per fork release, named "<core>_libretro.dylib.zip".
    // No substring requirement. mGBA has no distribution source at all
    // (no github_repo / core_buildbot_path — local universal build only),
    // so it never reaches matchAsset and this rule is inert for it.
    // Field order is {substrings, extension} (see EmulatorAdapter::AssetMatchRule).
    return { AssetMatchRule{ /*substrings*/ {}, /*extension*/ ".dylib.zip" } };
}

QString LibretroAdapter::findResumeFile(const QString& /*serial*/) const {
    // Concrete adapters override; libretro resume uses ROM-base-name + ".resume"
    // and is resolved at start time via the StartConfig.resumeStatePath.
    return {};
}

OptionsStore* LibretroAdapter::libretroOptionsStore() {
    // Live runtime owns its own store and is the source of truth while a
    // game is running.
    if (m_runtime) return &m_runtime->options();

    // Fallback: lazy-init a persistent store loaded from options.json.
    // Declared options are synthesized from the SettingDef schema so the
    // settings dialog can read & write libretro options without a running
    // core. Both stores share the same JSON file on disk, so the runtime
    // will see these edits on its next launch.
    if (!m_persistentOptions) {
        m_persistentOptions = std::make_unique<OptionsStore>();
        QVector<CoreOption> declared;
        for (const auto& def : settingsSchema()) {
            if (def.storage != SettingDef::Storage::LibretroOption) continue;
            CoreOption opt;
            opt.key = def.key;
            opt.label = def.label;
            opt.defaultValue = def.defaultValue;
            for (const auto& pair : def.options)
                opt.values.append(pair.second);
            declared.append(opt);
        }
        m_persistentOptions->load(optionsJsonPath(), declared);
    }
    return m_persistentOptions.get();
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
    // Drop the no-runtime fallback store: the runtime now owns the
    // authoritative store. Next no-runtime access (after releaseRuntime)
    // will lazily reload from disk and pick up any edits the runtime made.
    m_persistentOptions.reset();
}

void LibretroAdapter::releaseRuntime() {
    if (m_runtime) {
        // SP3.5: CoreRuntime is a QObject. Schedule deletion via deleteLater
        // instead of immediate destruction. This matters because the finished
        // signal that triggers this release dispatches on the main thread
        // while CoreRuntime::stop() is pumping the event loop from inside
        // itself — an immediate delete frees `this`, and the next iteration
        // of stop()'s while loop dereferences m_thread on a freed CoreRuntime,
        // crashing. With deleteLater the CoreRuntime stays alive until the
        // next event-loop iteration, by which point stop() has returned
        // cleanly.
        m_runtime.release()->deleteLater();
    }
    // Force reload-on-next-access so we don't return stale in-memory state
    // (the runtime may have written new values to options.json before exit).
    m_persistentOptions.reset();
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
