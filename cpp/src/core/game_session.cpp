#include "game_session.h"
#include "paths.h"
#include "system_registry.h"
#include "adapters/emulator_adapter.h"
#include "adapters/libretro/libretro_adapter.h"
#include "core/ini_file.h"
#include "core/libretro/frontend_settings_store.h"
#include "core/libretro/input_router.h"
#include "core/libretro/rcheevos_runtime.h"
#include "core/libretro/video_hardware_gl.h"
#include "core/libretro/libretro_render_surface.h"
#include "core/path_overrides_store.h"
#include "core/platform/dylib_arch.h"
#include "core/platform/host_arch.h"
#include "core/sdl_input_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QRegularExpression>

GameSession::GameSession(QObject* parent)
    : QObject(parent) {}

GameSession::~GameSession() = default;

bool GameSession::start(const EmulatorManifest& manifest,
                        EmulatorAdapter* adapter,
                        const QString& romPath,
                        const QString& systemId) {
    if (isRunning()) {
        qWarning() << "[GameSession] Already running";
        return false;
    }
    if (!QFileInfo::exists(romPath)) {
        emit errorOccurred("ROM file not found: " + romPath);
        return false;
    }

    // The game's real system. Falling back to the manifest's first system
    // (systemIdFor) keeps single-system cores working when the caller
    // doesn't pass one; multi-system cores (mGBA) rely on the passed value.
    m_systemId = systemId.isEmpty()
        ? Paths::systemIdFor(manifest.id, manifest.systems) : systemId;
    const QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    const QString dataPath = QFileInfo(Paths::emulatorDataDir(manifest.id, m_systemId)).absoluteFilePath();
    QDir().mkpath(dataPath);

    if (!adapter->ensureConfig(manifest, biosPath, dataPath))
        qWarning() << "[GameSession] ensureConfig failed for" << manifest.name;

    m_adapter = adapter;
    m_manifest = &manifest;
    m_emuId = manifest.id;
    m_currentRomPath = romPath;

    // Process-era retirement (2026-07): every emulator is an in-process
    // libretro core. The loader already rejects non-libretro manifests;
    // this is the belt to that suspender.
    if (manifest.backend != QLatin1String("libretro")) {
        emit errorOccurred(manifest.name + " has a non-libretro backend; "
                           "process-backend launching was retired.");
        return false;
    }
    return startLibretro(manifest, adapter, romPath);
}

bool GameSession::startLibretro(const EmulatorManifest& manifest,
                                EmulatorAdapter* adapter,
                                const QString& romPath) {
    auto* lr = dynamic_cast<LibretroAdapter*>(adapter);
    if (!lr) { emit errorOccurred("Adapter is not LibretroAdapter"); return false; }
    m_libretroAdapter = lr;

    // SP10 (generalized in packet 6): warn when the manifest-declared core
    // architecture doesn't match the running process arch. Manifest-driven
    // via core_arch instead of hardcoding emulator ids:
    //   x86_64 core + arm64 host → the distributed core hits the
    //     interpreter ceiling (pcsx2, ~65-70% speed) — advise Rosetta.
    //   arm64 core + Rosetta host → dlopen has no matching slice.
    // "universal" or undeclared core_arch never warns. Dismissable per
    // session. Reuses the existing generic info toast plumbing →
    // AchievementToast QML.
    //
    // core_arch describes the DISTRIBUTED artifact; the dylib on disk wins
    // when the two disagree (native-arm transition: a universal/native core
    // may be installed ahead of the manifest catching up). Probe the
    // installed file and skip the advice when it already has our slice.
    if (!m_slowModeNoticeShown
        && !DylibArch::fileContainsHostArch(
               LibretroAdapter::coreDylibPath(manifest))) {
        if (manifest.core_arch == QLatin1String("x86_64") && HostArch::isArm64()) {
            m_slowModeNoticeShown = true;
            emit raInfoToast(
                QStringLiteral("Performance"),
                QStringLiteral("%1 is faster under Rosetta").arg(manifest.name),
                QStringLiteral("Quit, right-click RetroNest in Finder → Get Info → "
                               "tick \"Open using Rosetta\", and relaunch."),
                QString(), 8000);
        } else if (manifest.core_arch == QLatin1String("arm64")
                   && HostArch::isRosettaX86_64()) {
            m_slowModeNoticeShown = true;
            emit raInfoToast(
                QStringLiteral("Architecture"),
                QStringLiteral("%1 is built for Apple Silicon only").arg(manifest.name),
                QStringLiteral("It can't load under Rosetta. Quit, right-click "
                               "RetroNest in Finder → Get Info → untick "
                               "\"Open using Rosetta\", and relaunch."),
                QString(), 8000);
        }
    }

    // SP3 + task #7: detect which HW render path this adapter uses and
    // surface the right string to QML's EmulationView Loader.
    //   MetalNSView  → "metal" (PCSX2 — LibretroMetalItem)
    //   GL           → "gl"    (PPSSPP — LibretroGLItem)
    //   None         → "software" (mGBA — LibretroVideoItem)
    const auto backend = lr->hardwareRenderBackend();
    const bool hw = backend != LibretroAdapter::HardwareRenderBackend::None;
    QString new_backend;
    switch (backend) {
        case LibretroAdapter::HardwareRenderBackend::MetalNSView:
            new_backend = QStringLiteral("metal"); break;
        case LibretroAdapter::HardwareRenderBackend::GL:
            new_backend = QStringLiteral("gl"); break;
        case LibretroAdapter::HardwareRenderBackend::None:
        default:
            new_backend = QStringLiteral("software"); break;
    }
    if (new_backend != m_libretroBackend) {
        m_libretroBackend = new_backend;
        emit libretroBackendChanged();
    }

    // Create the runtime BEFORE emitting aboutToStartLibretro: the QML
    // handler for that signal pushes EmulationView synchronously, which
    // makes LibretroMetalItem's Component.onCompleted fire registerHardware-
    // View(...) → m_libretroAdapter->runtime()->setActiveNSView(...). If
    // the runtime doesn't exist yet, that call is silently dropped and the
    // spin-wait below times out even though QML did its job.
    lr->prepareRuntime();
    auto* rt = lr->runtime();
    if (rt)
        rt->setHotkeyMatcher(m_hotkeyMatcher);

    // SP3 ordering fix: announce the libretro launch BEFORE we call rt->start
    // (which dlopens the core and runs retro_load_game → AcquireRenderWindow).
    // AppController/AppWindow react by pushing EmulationView; the Metal-backed
    // LibretroMetalItem then realises its NSView and registers it with
    // CoreRuntime via registerHardwareView(), so by the time the spin-wait
    // below resolves, RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW returns the
    // correct pointer to the core.
    emit aboutToStartLibretro();

    // NSView registration happens from QML once LibretroMetalItem is realized.
    // For now, ensure the runtime knows there's no view registered yet for
    // software backends (defensive — clears any stale value from a prior game).
    if (rt && !hw) {
        rt->setActiveNSView(nullptr);
    }

    const QString& systemId = m_systemId;

    CoreRuntime::StartConfig cfg;
    cfg.emuId    = manifest.id;
    cfg.glFlipPresentY = lr->glPresentFlipY();
    cfg.corePath = lr->resolveExecutable(manifest, Paths::emulatorsDir(manifest.install_folder));
    cfg.romPath = romPath;
    // System dir: shared bios/ by default, unless the adapter ships its
    // own asset tree next to the dylib (PPSSPP's ppsspp_libretro_resources
    // — see PpssppLibretroAdapter::systemDirOverride for the layouts).
    cfg.systemDir = lr->systemDirOverride();
    if (cfg.systemDir.isEmpty())
        cfg.systemDir = Paths::biosDir();
    cfg.saveDir = Paths::emulatorDataDir(manifest.id, systemId);
    cfg.optionsJsonPath = Paths::emulatorsDir("libretro") + "/" + lr->coreId() + "/options.json";

    // Phase E: build the schema-defaults override map from the libretro
    // adapter's settingsSchema(). Filters for Storage::LibretroOption rows
    // only — FrontendSetting rows (e.g. aspect_mode) live in a separate
    // sidecar and aren't libretro core options. Duplicate keys in the
    // schema (Recommended card duplicates rows from other cards) are
    // expected to carry matching defaultValue; the dupe-consistency test
    // enforces that invariant.
    {
        QHash<QString, QString> schemaDefaults;
        for (const auto& s : lr->settingsSchema()) {
            if (s.storage == SettingDef::Storage::LibretroOption && !s.key.isEmpty())
                schemaDefaults.insert(s.key, s.defaultValue);
        }
        cfg.schemaOptionDefaults = std::move(schemaDefaults);
    }

    // RA console id comes from the system registry (adapter overrides
    // retired in packet 7 stage 3). rcheevos treats 0 as "unknown console".
    const int raId = SystemRegistry::raConsoleId(systemId);
    cfg.raConsoleId = raId > 0 ? raId : 0;
    if (m_raConfig.valid) {
        cfg.raUsername = m_raConfig.username;
        if (m_raConfig.loginToken.isEmpty() && !m_raConfig.apiKey.isEmpty()) {
            qInfo() << "[GameSession] No libretro RA login token; achievement unlocks will not "
                       "be sent. Sign in via Settings -> RetroAchievements -> Sign in for "
                       "libretro achievements.";
        }
        cfg.raToken    = m_raConfig.loginToken;
        cfg.raHardcore = m_raConfig.hardcore;
        cfg.raEncore   = m_raConfig.encore;
    }

    // Fix 4: Populate resume state path. Key by serial, or the ROM base name
    // when serial extraction fails (e.g. RVZ, whose disc header the host can't
    // parse) — mirrors the save side in GameSession::terminate, which uses the
    // same serial-or-basename key. Without the fallback, RVZ titles never
    // resume because the lookup is skipped on the empty serial.
    const QString serial = lr->extractSerial(romPath);
    const QString resumeKey =
        serial.isEmpty() ? QFileInfo(romPath).completeBaseName() : serial;
    cfg.resumeStatePath = lr->findResumeFile(resumeKey);

    // Qt::UniqueConnection requires the slot to be a member function pointer
    // (qobject.h:267 asserts on lambdas + UniqueConnection). The runtime is
    // recreated per session via prepareRuntime/releaseRuntime, so duplicate
    // connections aren't possible here and AutoConnection is sufficient.
    connect(rt, &CoreRuntime::started, this, [this] {
        emit runningChanged(); emit started();
    });
    connect(rt, &CoreRuntime::finished, this, [this](bool crashed) {
        // Fix 1: restore navigation mode when the libretro game ends
        if (m_sdlInputManager)
            m_sdlInputManager->clearEmulationMode();
        // Tear down state BEFORE emitting runningChanged / finished so
        // QML observers reading `app.gameRunning` from the slot see
        // false (isRunning() is computed live from m_libretroAdapter).
        if (m_libretroAdapter) m_libretroAdapter->releaseRuntime();
        m_libretroAdapter = nullptr;
        const bool wasFastForward = m_libretroFastForward;
        m_libretroFastForward = false;
        if (wasFastForward) emit libretroFastForwardChanged();
        // Reset session-scoped libretro state so the next game's setters
        // (CoreRuntime → setLibretroAspectRatio after av_info read) fire
        // their change signals even if the new game reports the same
        // backend/aspect as the previous one. Pre-fence the PPSSPP Quit
        // crash forced an app restart between games; now relaunch stays in
        // the same process and stale equality short-circuits the QML
        // rewire chain (black screen on second launch). Sentinel value
        // -1.0 will never qFuzzyCompare-equal a real aspect (libretro
        // reports 0 for "no aspect" and positive values for real ones).
        m_libretroAspectRatio = -1.0;
        m_libretroBackend = QStringLiteral("software");
        m_systemId.clear();
        m_adapter = nullptr; m_manifest = nullptr;
        emit runningChanged();
        emit finished(crashed ? -1 : 0, crashed);
    });
    connect(rt, &CoreRuntime::errorOccurred, this, [this](const QString& m) {
        emit errorOccurred(m);
    });
    connect(rt, &CoreRuntime::aspectRatioReported,
            this, &GameSession::setLibretroAspectRatio);
    connect(rt, &CoreRuntime::frameReady, this, &GameSession::frameReady,
            Qt::UniqueConnection);

    // Forward achievement unlocks via our own signal — services-layer wiring
    // reconnects this to RAService::notifyAchievementUnlocked. Keeps core/
    // free of services/ includes.
    connect(&rt->rcheevos(), &RcheevosRuntime::achievementUnlocked,
            this, &GameSession::achievementUnlocked,
            Qt::QueuedConnection);
    // Same forwarding for the generic info-toast signal (game-start
    // banner, game-mastered, hardcore reset, server-error notice).
    connect(&rt->rcheevos(), &RcheevosRuntime::raInfoToast,
            this, &GameSession::raInfoToast,
            Qt::QueuedConnection);
    // And for the indicator-bar updates (challenge/progress chips,
    // connection status).
    connect(&rt->rcheevos(), &RcheevosRuntime::raIndicator,
            this, &GameSession::raIndicator,
            Qt::QueuedConnection);

    // Surface core-emitted OSD messages (RETRO_ENVIRONMENT_SET_MESSAGE /
    // SET_MESSAGE_EXT) through the existing toast pipeline. Header is the
    // emulator's display name so the user can tell which core is talking;
    // title carries the core's text. Duration falls back to a 4 s default
    // when the core asks for "frontend default" (durationMs == 0).
    const QString toastHeader = manifest.name;
    connect(rt, &CoreRuntime::coreMessage, this,
            [this, toastHeader](const QString& text, int durationMs) {
                emit raInfoToast(toastHeader, text, QString(), QString(),
                                 durationMs > 0 ? durationMs : 4000);
            },
            Qt::QueuedConnection);

    // Wire frontend settings changes → libretroFrontendChanged so QML
    // bindings on libretroAspectMode / libretroIntegerScale update live.
    if (auto* fs = lr->frontendSettingsStore()) {
        connect(fs, &FrontendSettingsStore::frontendSettingChanged,
                this, [this](const QString& /*key*/, const QString& /*value*/) {
                    emit libretroFrontendChanged();
                }, Qt::QueuedConnection);
    }
    // Emit once at game-start so QML reactive bindings get the current value
    // immediately (before any change signal fires).
    emit libretroFrontendChanged();

    // SP3 ordering fix: for the metal backend, pump the Qt event loop until
    // LibretroMetalItem has been instantiated by EmulationView's Loader and
    // its Component.onCompleted has called registerHardwareView() (which
    // populates CoreRuntime::activeNSView). Without this wait, rt->start
    // would call retro_load_game → Host::AcquireRenderWindow with the
    // NSView pointer still null, and GS device init would fail.
    //
    // Bounded by an elapsed-time deadline so a misconfigured QML stack
    // (e.g. EmulationView.qml missing, registerHardwareView never called)
    // surfaces as a clean error instead of an infinite spin.
    //
    // Task #7: the spin-wait is MetalNSView-specific — the GL backend
    // doesn't use NSView at all (it goes via SET_HW_RENDER + FBO),
    // and the VideoHardwareGL it does need is created lazily inside
    // installHwRender DURING retro_load_game (so waiting for it before
    // start() would deadlock). Skip the spin-wait for GL.
    const bool needsNSViewWait =
        backend == LibretroAdapter::HardwareRenderBackend::MetalNSView;
    if (needsNSViewWait) {
        qInfo() << "[GameSession] spin-wait: waiting for LibretroMetalItem NSView registration";
        constexpr int kHardwareViewWaitMs = 2000;
        QElapsedTimer waitTimer;
        waitTimer.start();
        while (!rt->activeNSView()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            if (waitTimer.elapsed() >= kHardwareViewWaitMs) {
                qWarning() << "[GameSession] Timed out after"
                           << kHardwareViewWaitMs
                           << "ms waiting for LibretroMetalItem NSView registration; "
                              "aborting libretro launch.";
                emit errorOccurred(QStringLiteral(
                    "Hardware render view was not ready in time. "
                    "Cannot start the libretro core."));
                // Match the rt->start-failure cleanup below.
                lr->releaseRuntime();
                m_libretroAdapter = nullptr;
                m_adapter = nullptr;
                m_manifest = nullptr;
                return false;
            }
        }
    }

    if (hw) {
        qInfo("[GameSession] spin-wait done: activeNSView=%p — calling rt->start",
              rt->activeNSView());
    }

    // SP5.5 followup: register the SdlInputManager BEFORE rt->start() spawns
    // the worker thread. QThread::start() provides a happens-before edge for
    // anything written before it, so the worker sees m_sdlInput as non-null
    // when PCSX2 queries the rumble interface during retro_init. Setting it
    // after start() would be a formal data race on the non-atomic pointer.
    if (m_sdlInputManager)
        rt->setSdlInputManager(m_sdlInputManager);

    if (!rt->start(cfg)) {
        // SP3 follow-up: on start failure the CoreRuntime::finished slot above
        // never fires (the runtime never reached the running state), so the
        // adapter pointer would otherwise stay set and subsequent launches
        // would hit "VM already running" / isRunning() == true.  Release the
        // runtime and clear state explicitly here.
        lr->releaseRuntime();
        m_libretroAdapter = nullptr;
        m_adapter = nullptr;
        m_manifest = nullptr;
        emit runningChanged();
        return false;
    }

    // NOTE: the GL software-fallback decision does NOT live here. rt->start()
    // spawns the worker and returns BEFORE retro_load_game (and thus
    // installHwRender) runs, so rt->videoHW() is always null at this point —
    // checking it here wrongly demoted every granted GL session to software
    // (black screen). The decision is made in setLibretroAspectRatio(),
    // which fires after retro_get_system_av_info (i.e. after installHwRender),
    // when videoHW() is definitive.

    // Populate the InputRouter from persisted bindings in controls.ini so that
    // user remappings are reflected at runtime. ensureConfig() seeds the file
    // with defaults before we ever reach this point.
    {
        InputRouter& router = rt->input();
        router.clearBindings();
        const int maxPlayers = lr->maxLibretroPlayers();
        const QString iniPath = lr->controllerBindingsConfigFilePath();
        IniFile ini;
        if (ini.load(iniPath)) {
            const QString section = lr->controllerBindingsSection(/*port=*/1);
            for (const auto& def : lr->controllerBindingDefsForType(/*type=*/{})) {
                const QString val = ini.value(section, def.key);
                if (val.isEmpty()) continue;
                // Parse "SDL-{idx}/{element}" — ignore non-SDL bindings
                if (!val.startsWith(QStringLiteral("SDL-"))) continue;
                const int slashAt = val.indexOf('/', 4);
                if (slashAt < 0) continue;
                bool ok = false;
                const int deviceIdx = val.mid(4, slashAt - 4).toInt(&ok);
                if (!ok) continue;
                const QString element = val.mid(slashAt + 1);
                const RetroPadSlot slot = retroPadSlotFromKey(def.key);
                if (slot == RetroPadSlot::None)
                    continue;
                // Bind the configured device, and replicate the same element->slot
                // mapping onto each additional player's device index so a 2nd/3rd
                // controller drives its own port with the standard layout. For
                // single-player cores (maxPlayers == 1) this binds only device 0,
                // exactly as before.
                router.bind(deviceIdx, element, slot);
                for (int p = 0; p < maxPlayers; ++p)
                    if (p != deviceIdx)
                        router.bind(p, element, slot);
            }
        } else {
            qWarning() << "[GameSession] Could not load controls.ini from" << iniPath
                       << "— controller input may not work";
        }
    }

    // Start routing SDL events into the InputRouter. Bindings have just
    // been loaded above; setSdlInputManager() was wired before rt->start()
    // so the rumble bridge already sees the manager.
    if (m_sdlInputManager)
        m_sdlInputManager->setEmulationMode(&rt->input());

    // Player 2+ support: tell the core which ports currently have a pad, and
    // keep it updated on hot-plug. Gated to multiplayer-capable cores so
    // single-player cores are unaffected. `rt` is the connection context, so
    // the lambda auto-disconnects when the runtime is destroyed on game end.
    if (m_sdlInputManager && lr->maxLibretroPlayers() > 1) {
        const int maxPlayers = lr->maxLibretroPlayers();
        SdlInputManager* sdl = m_sdlInputManager;
        auto syncPorts = [rt, sdl, maxPlayers]() {
            const QList<int> connected = sdl->connectedDeviceIndices();
            for (int port = 0; port < maxPlayers; ++port) {
                const bool present = connected.contains(port);
                rt->requestControllerPortDevice(static_cast<unsigned>(port),
                                                 present ? RETRO_DEVICE_JOYPAD : RETRO_DEVICE_NONE);
            }
        };
        syncPorts();  // initial snapshot at boot
        // Queued (not the default direct) connection: openController/closeController
        // emit controllersChanged() while holding SdlInputManager's controller mutex,
        // and syncPorts -> connectedDeviceIndices() re-locks that same mutex. A direct
        // call would deadlock (non-recursive mutex, same thread); queuing defers
        // syncPorts to the main-thread event loop after the lock is released.
        QObject::connect(sdl, &SdlInputManager::controllersChanged, rt, syncPorts,
                         Qt::QueuedConnection);
    }

    return true;
}

void GameSession::kill() {
    preShutdownRenderFence();   // no-op for non-GL paths
    if (m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->stop();
}

void GameSession::terminate() {
    preShutdownRenderFence();   // no-op for non-GL paths
    if (m_libretroAdapter && m_libretroAdapter->runtime()) {
        // Save-on-quit: pause the runtime, write resume file, then stop
        const auto* mf = m_manifest;
        if (mf) {
            const QString& systemId = m_systemId;
            // Use the serial as the resume filename so findResumeFile can match
            // by serial without a DB lookup.  Fall back to the ROM base name
            // only when serial extraction fails (e.g. unsupported format).
            const QString serial = m_libretroAdapter->extractSerial(m_currentRomPath);
            const QString resumeName = serial.isEmpty()
                ? QFileInfo(m_currentRomPath).completeBaseName()
                : serial;
            QString dir = PathOverridesStore::instance().read(mf->id, "SaveStates");
            if (dir.isEmpty())
                dir = Paths::emulatorDataDir(mf->id, systemId) + "/savestates";
            const QString resumePath = dir + "/" + resumeName + ".resume";
            QDir().mkpath(dir);
            // Schedule the save onto the worker thread (race-free: the worker
            // will flush the pending path in its post-loop teardown, before
            // retro_unload_game, after stop() unblocks the pause condvar).
            m_libretroAdapter->runtime()->requestSaveState(resumePath);
        }
        m_libretroAdapter->runtime()->stop();
    }
}

CoreRuntime* GameSession::libretroRuntime() const {
    if (!isRunning() || !m_libretroAdapter) return nullptr;
    return m_libretroAdapter->runtime();
}

void GameSession::setHotkeyMatcher(HotkeyMatcher* matcher) {
    m_hotkeyMatcher = matcher;
    // Forward to a live runtime immediately so clearing (nullptr at app
    // teardown) takes effect without waiting for the next session start.
    if (m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->setHotkeyMatcher(matcher);
}

void GameSession::pauseEmulation() {
    if (m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->pause();
    if (m_sdlInputManager)
        m_sdlInputManager->clearEmulationMode();
}

void GameSession::resumeEmulation() {
    if (m_libretroAdapter && m_libretroAdapter->runtime()) {
        if (m_sdlInputManager) {
            m_libretroAdapter->runtime()->setSdlInputManager(m_sdlInputManager);
            m_sdlInputManager->setEmulationMode(&m_libretroAdapter->runtime()->input());
        }
        m_libretroAdapter->runtime()->resume();
    }
}

QString GameSession::libretroSlotPath(int slot) const {
    if (!m_libretroAdapter || !m_manifest || m_currentRomPath.isEmpty())
        return {};
    const QString& systemId = m_systemId;
    const QString serial = m_libretroAdapter->extractSerial(m_currentRomPath);
    const QString baseName = serial.isEmpty()
        ? QFileInfo(m_currentRomPath).completeBaseName()
        : serial;
    // Path overrides: user-chosen dir trumps the default. The override
    // is stored per-emulator ("pcsx2" / "mgba") under the "SaveStates"
    // key; default is <emulator_data>/savestates.
    QString dir = PathOverridesStore::instance().read(m_manifest->id, "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir(m_manifest->id, systemId) + "/savestates";
    QDir().mkpath(dir);
    return dir + "/" + baseName + "_slot" + QString::number(slot) + ".state";
}

void GameSession::saveStateLibretro(int slot) {
    if (!m_libretroAdapter || !m_libretroAdapter->runtime()) return;
    const QString path = libretroSlotPath(slot);
    if (path.isEmpty()) return;
    m_libretroAdapter->runtime()->requestSaveState(path);
    emit stateSaveRequested();
}

void GameSession::loadStateLibretro(int slot) {
    if (!m_libretroAdapter || !m_libretroAdapter->runtime()) return;
    const QString path = libretroSlotPath(slot);
    if (path.isEmpty()) return;
    m_libretroAdapter->runtime()->requestLoadState(path);
    emit stateLoadRequested();
}

void GameSession::setCurrentSaveSlot(int slot) {
    if (slot < 1) slot = 1;
    if (slot > 5) slot = 5;
    if (slot == m_currentSaveSlot) return;
    m_currentSaveSlot = slot;
    emit currentSaveSlotChanged();
}

bool GameSession::toggleFastForwardLibretro() {
    if (!m_libretroAdapter || !m_libretroAdapter->runtime()) return false;
    m_libretroFastForward = !m_libretroFastForward;
    m_libretroAdapter->runtime()->setSpeedMultiplier(m_libretroFastForward ? 2.0 : 1.0);
    emit libretroFastForwardChanged();
    return m_libretroFastForward;
}

void GameSession::registerHardwareView(qulonglong view_ptr) {
    qInfo("[GameSession] registerHardwareView(0x%llx) adapter=%p runtime=%p",
          view_ptr,
          static_cast<void*>(m_libretroAdapter),
          m_libretroAdapter ? static_cast<void*>(m_libretroAdapter->runtime()) : nullptr);
    if (!m_libretroAdapter) return;
    auto* rt = m_libretroAdapter->runtime();
    if (!rt) return;
    rt->setActiveNSView(reinterpret_cast<void*>(view_ptr));
}

void GameSession::registerLibretroGLItem(QObject* item) {
    // dynamic_cast recovers the render-surface interface from the QObject
    // (LibretroGLItem multiply-inherits QQuickItem + LibretroRenderSurface).
    // QPointer<QObject> self-clears on destruction, so the explicit null
    // call from QML's Component.onDestruction is belt-and-suspenders.
    m_renderSurfaceObj = item;
    m_renderSurface = dynamic_cast<LibretroRenderSurface*>(item);
    if (item && !m_renderSurface) {
        qWarning() << "[GameSession] registerLibretroGLItem: object is not a "
                      "LibretroRenderSurface, ignoring";
        m_renderSurfaceObj = nullptr;
    }
}

void GameSession::preShutdownRenderFence() {
    // Only the libretro GL backend has the IOSurface→MTLTexture coupling
    // that races against worker-side VideoHardwareGL teardown. Software
    // (mGBA) and Metal-direct (PCSX2 libretro) paths skip this entirely.
    // The Qt Quick / scene-graph specifics live in the surface impl
    // (LibretroGLItem::fenceForShutdown) so core/ stays Quick-free (P10).
    if (m_libretroBackend != QStringLiteral("gl")) return;
    if (m_renderSurfaceObj && m_renderSurface)
        m_renderSurface->fenceForShutdown();
}

QObject* GameSession::videoHardware() const {
    if (!m_libretroAdapter) return nullptr;
    auto* rt = m_libretroAdapter->runtime();
    if (!rt) return nullptr;
    // VideoHardwareGL inherits from QObject; static_cast for QML to see the
    // signals (frameReady) when LibretroGLItem calls setVideoHardware.
    return static_cast<QObject*>(rt->videoHW());
}

bool GameSession::isRunning() const {
    return m_libretroAdapter && m_libretroAdapter->runtime();
}

QString GameSession::detectedGameSerial() const {
    if (m_libretroAdapter && m_libretroAdapter->runtime())
        return m_libretroAdapter->runtime()->detectedGameSerial();
    return {};
}

QString GameSession::libretroAspectMode() const {
    if (!m_libretroAdapter) return QStringLiteral("native");
    if (auto* store = m_libretroAdapter->frontendSettingsStore()) {
        const QString v = store->get(QStringLiteral("aspect_mode"));
        return v.isEmpty() ? QStringLiteral("native") : v;
    }
    return QStringLiteral("native");
}

bool GameSession::libretroIntegerScale() const {
    if (!m_libretroAdapter) return false;
    if (auto* store = m_libretroAdapter->frontendSettingsStore())
        return store->get(QStringLiteral("integer_scale")).compare("ON", Qt::CaseInsensitive) == 0;
    return false;
}

void GameSession::setLibretroAspectRatio(qreal ratio) {
    // This fires after retro_get_system_av_info, i.e. AFTER the core's
    // SET_HW_RENDER / installHwRender has run on the worker — the first
    // moment rt->videoHW() is definitive. So this is where we reconcile the
    // optimistic GL backend guess with reality: if we picked GL but the
    // context wasn't granted (e.g. PPSSPP's legacy "OpenGL" asks for
    // RETRO_HW_CONTEXT_OPENGL, which this bridge declines — only
    // OPENGL_CORE), the core silently ran software. Flip the video backend
    // to software so EmulationView swaps the GL item for the software one
    // and shows the core's frames instead of black against absent hardware.
    // (The granted case leaves videoHW() non-null → no switch → the GL item
    // rewires to it via the libretroAspectRatioChanged emitted below.)
    if (m_libretroBackend == QStringLiteral("gl")
            && m_libretroAdapter && m_libretroAdapter->runtime()
            && !m_libretroAdapter->runtime()->videoHW()) {
        qInfo() << "[GameSession] GL hardware render was not granted; core is "
                   "running software — switching video backend to software";
        m_libretroBackend = QStringLiteral("software");
        emit libretroBackendChanged();
    }

    // CoreRuntime calls this once per session after retro_get_system_av_info,
    // and again whenever the core re-emits SET_SYSTEM_AV_INFO.
    //
    // ratio > 0  → explicit aspect (e.g. 4/3, 16/9, custom-from-patch).
    // ratio == 0 → libretro convention: "no aspect specified."
    //              LibretroMetalItem treats this as fill-the-bounds
    //              (Stretch semantics). The pcsx2-libretro core emits 0.0
    //              when its pcsx2_aspect_ratio option is set to Stretch.
    //
    // qFuzzyCompare avoids a useless emit when the core re-reports the
    // same value (e.g. SP7a's region-refinement re-emit pass).
    if (qFuzzyCompare(m_libretroAspectRatio, ratio)) return;
    m_libretroAspectRatio = ratio;
    emit libretroAspectRatioChanged();
}
