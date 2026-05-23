#include "game_session.h"
#include "paths.h"
#include "adapters/emulator_adapter.h"
#include "adapters/libretro/libretro_adapter.h"
#include "core/ini_file.h"
#include "core/libretro/frontend_settings_store.h"
#include "core/libretro/input_router.h"
#include "core/libretro/rcheevos_runtime.h"
#include "core/libretro/video_hardware_gl.h"
#include "ui/libretro/libretro_gl_item.h"
#include "core/path_overrides_store.h"
#include "core/platform/host_arch.h"
#include "core/sdl_input_manager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTimer>

GameSession::GameSession(QObject* parent)
    : QObject(parent) {}

GameSession::~GameSession() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool GameSession::start(const EmulatorManifest& manifest,
                        EmulatorAdapter* adapter,
                        const QString& romPath,
                        const QStringList& extraArgs) {
    if (isRunning()) {
        qWarning() << "[GameSession] Already running";
        return false;
    }
    if (!QFileInfo::exists(romPath)) {
        emit errorOccurred("ROM file not found: " + romPath);
        return false;
    }

    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);
    const QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    const QString dataPath = QFileInfo(Paths::emulatorDataDir(manifest.id, systemId)).absoluteFilePath();
    QDir().mkpath(dataPath);

    if (!adapter->ensureConfig(manifest, biosPath, dataPath))
        qWarning() << "[GameSession] ensureConfig failed for" << manifest.name;

    m_adapter = adapter;
    m_manifest = &manifest;
    m_emuId = manifest.id;
    m_currentRomPath = romPath;

    if (manifest.backend == "libretro") {
        m_backend = Backend::Libretro;
        return startLibretro(manifest, adapter, romPath);
    }
    m_backend = Backend::Process;
    return startProcess(manifest, adapter, romPath, extraArgs);
}

bool GameSession::startProcess(const EmulatorManifest& manifest,
                               EmulatorAdapter* adapter,
                               const QString& romPath,
                               const QStringList& extraArgs) {
    // Resolve executable
    const QString installPath = Paths::emulatorsDir(manifest.install_folder);
    const QString execPath = QFileInfo(adapter->resolveExecutable(manifest, installPath)).absoluteFilePath();

    if (!QFileInfo::exists(execPath)) {
        emit errorOccurred(manifest.name + " is not installed. Executable not found: " + execPath);
        return false;
    }

    // Build arguments
    QStringList args = adapter->buildLaunchArgs(manifest, romPath);
    if (!extraArgs.isEmpty()) {
        // Insert extra args before "--" separator (if present) so they're
        // treated as flags, not positional arguments / filenames
        int sepIdx = args.indexOf("--");
        if (sepIdx >= 0) {
            for (int i = extraArgs.size() - 1; i >= 0; --i)
                args.insert(sepIdx, extraArgs[i]);
        } else {
            args.append(extraArgs);
        }
    }

    // Resolve working directory
    QString cwd;
#if defined(Q_OS_MACOS)
    static const QRegularExpression appRe("^(.+\\.app)/");
    auto match = appRe.match(execPath);
    if (match.hasMatch()) {
        cwd = QFileInfo(match.captured(1)).absolutePath();
    }
#endif
    if (cwd.isEmpty()) {
        cwd = QFileInfo(execPath).absolutePath();
    }

    // Create and configure process
    delete m_process;
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(cwd);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::finished, this, &GameSession::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GameSession::onProcessError);
    connect(m_process, &QProcess::readyRead, this, &GameSession::onReadyRead);

    qInfo().noquote() << "[GameSession]" << manifest.name << ":" << execPath << args.join(" ");
    qInfo().noquote() << "[GameSession] CWD:" << cwd;

    m_process->start(execPath, args);

    if (!m_process->waitForStarted(5000)) {
        emit errorOccurred("Failed to start process: " + m_process->errorString());
        return false;
    }

    qInfo() << "[GameSession] PID:" << m_process->processId();
    emit runningChanged();
    emit started();
    return true;
}

bool GameSession::startLibretro(const EmulatorManifest& manifest,
                                EmulatorAdapter* adapter,
                                const QString& romPath) {
    auto* lr = dynamic_cast<LibretroAdapter*>(adapter);
    if (!lr) { emit errorOccurred("Adapter is not LibretroAdapter"); return false; }
    m_libretroAdapter = lr;

    // SP10: warn the user when launching the PS2 libretro core in arm64
    // mode — they'll get the interpreter ceiling (~65-70% speed) instead
    // of recompiler speed. Dismissable per session. Reuses the existing
    // generic info toast plumbing → AchievementToast QML.
    if (HostArch::isArm64() && lr->coreId() == QStringLiteral("pcsx2")
        && !m_slowModeNoticeShown) {
        m_slowModeNoticeShown = true;
        emit raInfoToast(
            QStringLiteral("Performance"),
            QStringLiteral("PS2 emulation is faster under Rosetta"),
            QStringLiteral("Quit, right-click RetroNest in Finder → Get Info → "
                           "tick \"Open using Rosetta\", and relaunch."),
            QString(), 8000);
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

    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);

    CoreRuntime::StartConfig cfg;
    cfg.emuId    = manifest.id;
    cfg.corePath = lr->resolveExecutable(manifest, Paths::emulatorsDir(manifest.install_folder));
    cfg.romPath = romPath;
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

    cfg.raConsoleId = lr->raConsoleId(systemId);
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

    // Fix 4: Populate resume state path
    const QString serial = lr->extractSerial(romPath);
    if (!serial.isEmpty())
        cfg.resumeStatePath = lr->findResumeFile(serial);

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
        m_libretroFastForward = false;
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

    // Populate the InputRouter from persisted bindings in controls.ini so that
    // user remappings are reflected at runtime. ensureConfig() seeds the file
    // with defaults before we ever reach this point.
    {
        InputRouter& router = rt->input();
        router.clearBindings();
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
                if (slot != RetroPadSlot::None)
                    router.bind(deviceIdx, element, slot);
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

    return true;
}

void GameSession::kill() {
    preShutdownRenderFence();   // no-op for non-GL paths
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->stop();
    else if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Killing emulator process";
        m_process->kill();
    }
}

void GameSession::terminate() {
    preShutdownRenderFence();   // no-op for non-GL paths
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime()) {
        // Save-on-quit: pause the runtime, write resume file, then stop
        const auto* mf = m_manifest;
        if (mf) {
            const QString systemId = Paths::systemIdFor(mf->id, mf->systems);
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
    } else if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Terminating emulator process (SIGTERM)";
        m_process->terminate();
    }
}

void GameSession::pauseEmulation() {
    if (m_backend != Backend::Libretro) return;
    if (m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->pause();
    if (m_sdlInputManager)
        m_sdlInputManager->clearEmulationMode();
}

void GameSession::resumeEmulation() {
    if (m_backend != Backend::Libretro) return;
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
    const QString systemId = Paths::systemIdFor(m_manifest->id, m_manifest->systems);
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
    if (m_backend != Backend::Libretro) return;
    if (!m_libretroAdapter || !m_libretroAdapter->runtime()) return;
    const QString path = libretroSlotPath(slot);
    if (path.isEmpty()) return;
    m_libretroAdapter->runtime()->requestSaveState(path);
}

void GameSession::loadStateLibretro(int slot) {
    if (m_backend != Backend::Libretro) return;
    if (!m_libretroAdapter || !m_libretroAdapter->runtime()) return;
    const QString path = libretroSlotPath(slot);
    if (path.isEmpty()) return;
    m_libretroAdapter->runtime()->requestLoadState(path);
}

void GameSession::setCurrentSaveSlot(int slot) {
    if (slot < 1) slot = 1;
    if (slot > 5) slot = 5;
    if (slot == m_currentSaveSlot) return;
    m_currentSaveSlot = slot;
    emit currentSaveSlotChanged();
}

bool GameSession::toggleFastForwardLibretro() {
    if (m_backend != Backend::Libretro) return false;
    if (!m_libretroAdapter || !m_libretroAdapter->runtime()) return false;
    m_libretroFastForward = !m_libretroFastForward;
    m_libretroAdapter->runtime()->setSpeedMultiplier(m_libretroFastForward ? 2.0 : 1.0);
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
    // qobject_cast returns nullptr if item is null or not a LibretroGLItem.
    // QPointer accepts that directly — the field self-clears on destruction
    // too, so the explicit null call from QML's Component.onDestruction is
    // belt-and-suspenders.
    m_libretroGLItem = qobject_cast<LibretroGLItem*>(item);
    if (item && !m_libretroGLItem) {
        qWarning() << "[GameSession] registerLibretroGLItem: object is not a "
                      "LibretroGLItem, ignoring";
    }
}

void GameSession::preShutdownRenderFence() {
    // Only the libretro GL backend has the IOSurface→MTLTexture coupling
    // that races against worker-side VideoHardwareGL teardown. Software
    // (mGBA) and Metal-direct (PCSX2 libretro) paths skip this entirely.
    if (m_libretroBackend != QStringLiteral("gl")) return;
    if (!m_libretroGLItem) return;

    LibretroGLItem* item = m_libretroGLItem.data();
    QQuickWindow* w = item->window();
    if (!w) {
        qWarning() << "[GameSession] preShutdownRenderFence: glItem has no "
                      "window, skipping fence (degraded — same risk as before "
                      "the fix)";
        return;
    }

    // Drop the LibretroGLItem's strong ARC ref to the MTLTexture and
    // disconnect its VideoHardwareGL signals. After the next sync,
    // updatePaintNode sees m_hw == nullptr and returns nullptr, deleting
    // the QSGSimpleTextureNode and its owned QSGTexture — releasing the
    // QSGMetalTexture wrapper's last strong ARC ref to the MTLTexture.
    item->setVideoHardware(nullptr);
    item->update();

    // Wait two render passes. Frame 1 covers the sync that processes the
    // cleared updatePaintNode and deletes the node + texture. Frame 2
    // covers any GPU command buffer that captured the MTLTexture before
    // the clear and is still draining on the GPU.
    QEventLoop loop;
    int framesSeen = 0;
    auto conn = QObject::connect(
        w, &QQuickWindow::afterRendering, &loop,
        [&framesSeen, &loop]() {
            if (++framesSeen >= 2) loop.quit();
        },
        Qt::QueuedConnection);   // afterRendering fires on QSGRenderThread

    // Hard cap — covers degenerate cases (window hidden, rendering paused,
    // app already quitting). At worst we're at the same risk as before the
    // fix; the cap doesn't make anything worse.
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();
    // Explicit disconnect before scope exit is load-bearing: the queued
    // lambda captures &loop and &framesSeen by reference. QObject::~QObject
    // does flush pending queued events for the context object in Qt 6, but
    // relying on that is an implementation detail. Disconnecting here
    // ensures no late delivery arrives after the locals are invalid.
    QObject::disconnect(conn);

    qInfo() << "[GameSession] preShutdownRenderFence drained" << framesSeen
            << "frame(s) before stop";
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
    if (m_backend == Backend::Libretro)
        return m_libretroAdapter && m_libretroAdapter->runtime();
    return m_process && m_process->state() != QProcess::NotRunning;
}

qint64 GameSession::pid() const {
    return m_process ? m_process->processId() : -1;
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

void GameSession::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool crashed = (exitStatus == QProcess::CrashExit);
    if (crashed) {
        qWarning() << "[GameSession]" << m_emuId << "crashed.";
    } else {
        qInfo() << "[GameSession]" << m_emuId << "exited with code" << exitCode;
    }
    m_adapter = nullptr;
    m_manifest = nullptr;
    emit runningChanged();
    emit finished(exitCode, crashed);
}

void GameSession::onProcessError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        emit errorOccurred("Process failed to start: " + m_process->errorString());
        m_adapter = nullptr;
        m_manifest = nullptr;
        emit runningChanged();
    }
}

void GameSession::onReadyRead() {
    QByteArray output = m_process->readAll();
    for (const auto& line : output.split('\n')) {
        if (!line.trimmed().isEmpty()) {
            qInfo().noquote() << "  [" + m_emuId + "]" << line.trimmed();
        }
    }
}
