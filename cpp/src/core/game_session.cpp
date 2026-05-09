#include "game_session.h"
#include "paths.h"
#include "adapters/emulator_adapter.h"
#include "adapters/libretro/libretro_adapter.h"
#include "core/ini_file.h"
#include "core/libretro/frontend_settings_store.h"
#include "core/libretro/input_router.h"
#include "core/libretro/rcheevos_runtime.h"
#include "core/sdl_input_manager.h"
#include "services/ra_service.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

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
    lr->prepareRuntime();
    auto* rt = lr->runtime();

    const QString systemId = Paths::systemIdFor(manifest.id, manifest.systems);

    CoreRuntime::StartConfig cfg;
    cfg.corePath = lr->resolveExecutable(manifest, Paths::emulatorsDir(manifest.install_folder));
    cfg.romPath = romPath;
    cfg.systemDir = Paths::biosDir();
    cfg.saveDir = Paths::emulatorDataDir(manifest.id, systemId);
    cfg.optionsJsonPath = Paths::emulatorsDir("libretro") + "/" + lr->coreId() + "/options.json";

    // Fix 3: Populate RA fields
    cfg.raConsoleId = lr->raConsoleId(systemId);
    if (m_raService) {
        cfg.raUsername = m_raService->credentials().username;
        if (m_raService->credentials().loginToken.isEmpty()
                && !m_raService->credentials().apiKey.isEmpty()) {
            qInfo() << "[GameSession] No libretro RA login token; achievement unlocks will not "
                       "be sent. Sign in via Settings -> RetroAchievements -> Sign in for "
                       "libretro achievements.";
        }
        cfg.raToken    = m_raService->credentials().loginToken;
        cfg.raHardcore = m_raService->hardcoreMode();
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
    connect(rt, &CoreRuntime::frameReady, this, &GameSession::frameReady,
            Qt::UniqueConnection);

    // Fix 5: Forward achievement unlocks to RAService
    if (m_raService) {
        connect(&rt->rcheevos(), &RcheevosRuntime::achievementUnlocked,
                m_raService, &RAService::notifyAchievementUnlocked,
                Qt::QueuedConnection);
    }

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

    if (!rt->start(cfg))
        return false;

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

    // Fix 1: Switch SDL input into emulation mode so button events feed the InputRouter
    if (m_sdlInputManager)
        m_sdlInputManager->setEmulationMode(&rt->input());

    return true;
}

void GameSession::kill() {
    if (m_backend == Backend::Libretro && m_libretroAdapter && m_libretroAdapter->runtime())
        m_libretroAdapter->runtime()->stop();
    else if (m_process && m_process->state() != QProcess::NotRunning) {
        qInfo() << "[GameSession] Killing emulator process";
        m_process->kill();
    }
}

void GameSession::terminate() {
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
            const QString resumePath = Paths::emulatorDataDir(mf->id, systemId)
                + "/savestates/" + resumeName + ".resume";
            QDir().mkpath(QFileInfo(resumePath).absolutePath());
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
        if (m_sdlInputManager)
            m_sdlInputManager->setEmulationMode(&m_libretroAdapter->runtime()->input());
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
    const QString path = Paths::emulatorDataDir(m_manifest->id, systemId)
        + "/savestates/" + baseName + "_slot" + QString::number(slot) + ".state";
    QDir().mkpath(QFileInfo(path).absolutePath());
    return path;
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

bool GameSession::toggleFastForwardLibretro() {
    if (m_backend != Backend::Libretro) return false;
    if (!m_libretroAdapter || !m_libretroAdapter->runtime()) return false;
    m_libretroFastForward = !m_libretroFastForward;
    m_libretroAdapter->runtime()->setSpeedMultiplier(m_libretroFastForward ? 4.0 : 1.0);
    return m_libretroFastForward;
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
