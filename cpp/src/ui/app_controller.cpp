#include "app_controller.h"
#include "adapters/adapter_registry.h"
#include "adapters/libretro/libretro_adapter.h"
#include "core/macos_fullscreen.h"
#include "core/paths.h"
#include "core/scraper.h"
#include "in_game_menu_panel.h"
#include "core/sdl_input_manager.h"

#include <QQmlEngine>
#include <QTimer>
#include <QPointer>

#include <QCursor>
#include "settings/controller_mapping_page.h"
#include "settings/pcsx2/pcsx2_settings_dialog.h"
#include "settings/duckstation/duckstation_settings_dialog.h"
#include "settings/ppsspp/ppsspp_settings_dialog.h"
#include "settings/dolphin/dolphin_settings_dialog.h"
#include "settings/mgba/mgba_settings_dialog.h"
#include "settings/hotkey_settings_dialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

AppController::AppController(ManifestLoader* loader, Database* db, QObject* parent)
    : QObject(parent)
    , m_loader(loader)
    , m_db(db)
    , m_gameService(loader, db)
    , m_scraperService(db)
    , m_emuService(loader)
    , m_raService(db)
    , m_configService(loader)
{
    connect(&m_configService, &ConfigService::statusMessage, this, &AppController::setStatus);
    connect(&m_configService, &ConfigService::configurationReset,
            this, &AppController::configurationReset);
    m_scraperService.loadCredentials();
    m_raService.loadCredentials();

    // Forward libretro achievement unlocks from GameSession (which lives in
    // core/) onto RAService (services/). GameSession itself has no
    // dependency on services — it just emits the signal it receives from
    // its rcheevos runtime.
    connect(m_gameService.session(), &GameSession::achievementUnlocked,
            &m_raService, &RAService::notifyAchievementUnlocked,
            Qt::QueuedConnection);
    // Same shape for the generic info toast (game-start, mastered,
    // hardcore reset, server error).
    connect(m_gameService.session(), &GameSession::raInfoToast,
            &m_raService, &RAService::notifyInfoToast,
            Qt::QueuedConnection);
    // And for the indicator-bar updates (challenge/progress chips,
    // connection status).
    connect(m_gameService.session(), &GameSession::raIndicator,
            &m_raService, &RAService::notifyIndicator,
            Qt::QueuedConnection);

    connect(&m_gameService, &GameService::statusMessage, this, &AppController::setStatus);
    connect(&m_gameService, &GameService::gameRunningChanged, this, &AppController::gameRunningChanged);
    connect(&m_gameService, &GameService::gameStarted, this, [this]() {
        if (m_gameService.session()->isLibretro())
            emit gameStartedLibretro();
        else
            emit gameStarted();
    });
    // SP3: forward GameSession's pre-start libretro signal so QML can push
    // EmulationView (and realise LibretroMetalItem's NSView) before
    // retro_load_game runs inside startLibretro().
    connect(m_gameService.session(), &GameSession::aboutToStartLibretro,
            this, &AppController::gameStartingLibretro);
    connect(&m_gameService, &GameService::gameFinished, this, &AppController::gameFinished);
    // Register global Cmd+Escape hotkey and wire to signal.
    // QPointer guards against use-after-free if AppController is ever
    // destroyed before MacFullscreen::unregisterGlobalHotkey runs (the
    // raw-pointer version was a hidden footgun).
    static QPointer<AppController> s_instance;
    s_instance = this;
    MacFullscreen::registerGlobalHotkey([]() {
        qInfo() << "[AppController] Cmd+Shift+Esc fired → globalHotkeyPressed";
        if (auto* inst = s_instance.data())
            emit inst->globalHotkeyPressed();
    });
    connect(&m_scraperService, &ScraperService::statusMessage, this, &AppController::setStatus);
    connect(&m_emuService, &EmulatorService::statusMessage, this, &AppController::setStatus);
    // validateAndSaveCredentials is async (worker thread) — translate the
    // service signal into a status update and the QML-facing signal here.
    connect(&m_scraperService, &ScraperService::credentialsValidated,
            this, [this](bool valid, const QString& message) {
                setStatus(valid ? "Connected to ScreenScraper." : "Invalid credentials.");
                emit scraperCredentialsValidated(valid, message);
            });
    connect(&m_scraperService, &ScraperService::scrapeProgress,
            this, &AppController::scrapeProgress);
    connect(&m_scraperService, &ScraperService::scrapeFinished,
            this, [this](int succeeded, int failed, int skipped) {
                emit scrapeFinished(succeeded, failed, skipped);
                emit gamesChanged();
            });

    // Forward emulator install/uninstall signals
    connect(&m_emuService, &EmulatorService::installProgress,
            this, &AppController::installProgress);
    connect(&m_emuService, &EmulatorService::installFinished, this,
        [this](const QString& emuId, bool success, const QString& message) {
            setStatus(message);
            if (success) emit emulatorInstalled(emuId);
            emit installFinished(emuId, success, message);
        });
    connect(&m_emuService, &EmulatorService::uninstallFinished, this,
        [this](const QString& emuId, bool success, const QString& message) {
            setStatus(message);
            if (success) emit emulatorInstalled(emuId);  // reuse to refresh UI
            emit uninstallFinished(emuId, success, message);
        });
    connect(&m_emuService, &EmulatorService::updateAvailable,
            this, &AppController::updateAvailable);

    // Forward RA signals
    connect(&m_raService, &RAService::loginCompleted, this, &AppController::raLoginCompleted);
    connect(&m_raService, &RAService::signedOut, this, &AppController::raSignedOut);
    connect(&m_raService, &RAService::loginTokenChanged, this, &AppController::raLoginTokenChanged);
    connect(&m_raService, &RAService::loginFailed, this, &AppController::raLoginFailed);
    connect(&m_raService, &RAService::userSummaryReady, this, &AppController::raUserSummaryReady);
    connect(&m_raService, &RAService::userGamesReady, this, &AppController::raUserGamesReady);
    connect(&m_raService, &RAService::gameDetailReady, this, &AppController::raGameDetailReady);
    connect(&m_raService, &RAService::gameIdLookupReady, this, &AppController::raGameIdLookupReady);
    // Forward libretro achievement unlocks to QML for the toast UI.
    // The signal chain that produces this:
    //   RcheevosRuntime → GameSession (signal forward, queued) →
    //   RAService::notifyAchievementUnlocked → RAService::achievementUnlocked
    //   → here → QML.
    connect(&m_raService, &RAService::achievementUnlocked,
            this, &AppController::raAchievementUnlocked);
    connect(&m_raService, &RAService::infoToast,
            this, &AppController::raInfoToast);
    connect(&m_raService, &RAService::indicator,
            this, &AppController::raIndicator);
}

// ── Game Session ───────────────────────────────────────────

GameSession* AppController::gameSession() {
    return m_gameService.session();
}

// ── App State ──────────────────────────────────────────────

QString AppController::statusMessage() const { return m_statusMessage; }

QStringList AppController::systems() const {
    return m_db->allSystems();
}

QString AppController::currentSystem() const { return m_currentSystem; }
void AppController::setCurrentSystem(const QString& sys) {
    if (m_currentSystem != sys) {
        m_currentSystem = sys;
        emit currentSystemChanged();
    }
}

int AppController::currentTab() const { return m_currentTab; }
void AppController::setCurrentTab(int tab) {
    if (m_currentTab != tab) {
        m_currentTab = tab;
        emit currentTabChanged();
    }
}

int AppController::settingsCategory() const { return m_settingsCategory; }
void AppController::setSettingsCategory(int cat) {
    if (m_settingsCategory != cat) {
        m_settingsCategory = cat;
        emit settingsCategoryChanged();
    }
}

void AppController::setStatus(const QString& msg) {
    m_statusMessage = msg;
    emit statusMessageChanged();
}

// ── Game Operations ────────────────────────────────────────

void AppController::importRoms() {
    QString dir = QFileDialog::getExistingDirectory(nullptr, "Choose ROM Folder",
        QDir::homePath(), QFileDialog::ShowDirsOnly);
    if (dir.isEmpty()) return;

    importRomsFromDir(dir, "");
}

void AppController::scanRomFolders() {
    auto result = m_gameService.scanRomFolders();
    setStatus(result.message);
    emit systemsChanged();
    emit gamesChanged();
}

void AppController::backfillSerials() {
    m_gameService.backfillSerials();
}

QStringList AppController::importableSystems() const {
    return m_gameService.importableSystems();
}

void AppController::importRomsFromDir(const QString& dir, const QString& systemFilter) {
    auto result = m_gameService.importRoms(dir, systemFilter);
    setStatus(result.message);
    emit systemsChanged();
    emit gamesChanged();
}

void AppController::launchGame(int /*gameId*/, const QString& romPath, const QString& emuId,
                               const QStringList& extraArgs) {
    // Check if we need to show a one-time RA login prompt BEFORE launching
    if (m_raService.hasCredentials()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (adapter && adapter->supportsRetroAchievements()) {
            if (m_raService.needsEmulatorLoginPrompt(emuId)) {
                auto* manifest = m_loader->emulatorById(emuId);
                QString name = (manifest && !manifest->name.isEmpty()) ? manifest->name : emuId;
                // Store pending launch info and show prompt — QML will call launchGame again after dismissal
                m_pendingLaunchRom = romPath;
                m_pendingLaunchEmu = emuId;
                m_pendingLaunchArgs = extraArgs;
                emit raEmulatorLoginPrompt(name);
                return;  // don't launch yet
            }

            adapter->patchRetroAchievements(
                "", "",
                true,
                m_raService.hardcoreMode(),
                m_raService.notifications(),
                m_raService.soundEffects());
        }
    }

    // Push current RA values into the GameSession before launch so the
    // libretro path can wire achievements without GameSession holding a
    // services/ dependency. No-op for external-process adapters.
    LibretroRaConfig raCfg;
    if (m_raService.hasCredentials()) {
        raCfg.username   = m_raService.credentials().username;
        raCfg.loginToken = m_raService.credentials().loginToken;
        raCfg.apiKey     = m_raService.credentials().apiKey;
        raCfg.hardcore   = m_raService.hardcoreMode();
        raCfg.encore     = m_raService.encoreMode();
        raCfg.valid      = true;
    }
    m_gameService.session()->setLibretroRaConfig(raCfg);

    if (!m_gameService.startGame(romPath, emuId, extraArgs)) {
        setStatus("Launch failed");
    }
}

bool AppController::isGameRunning() const {
    return m_gameService.isGameRunning();
}

void AppController::stopGame() {
    m_gameService.stopGame();
}

void AppController::saveAndStopGame(int slot) {
    m_gameService.saveAndStopGame(slot);
}

bool AppController::hasResumeState(const QString& romPath, const QString& emuId) {
    return m_gameService.hasResumeState(romPath, emuId);
}

QString AppController::resumeStateFile(const QString& romPath, const QString& emuId) {
    return m_gameService.resumeStateFile(romPath, emuId);
}

void AppController::clearResumeState(const QString& romPath, const QString& emuId) {
    m_gameService.clearResumeState(romPath, emuId);
}

void AppController::activateApp() {
    MacFullscreen::activateOurApp();
}

void AppController::removeGame(int gameId) {
    m_gameService.removeGame(gameId);
    setStatus("Game removed.");
    emit systemsChanged();
    emit gamesChanged();
}

void AppController::scrapeGame(int gameId) {
    // Delegates to the async single-game path. The scrapeFinished handler
    // in our constructor emits gamesChanged() when the worker completes.
    setStatus("Scraping game...");
    m_scraperService.startSingleGameScrape(gameId);
}

void AppController::scrapeGameWithProgress(int gameId) {
    m_scraperService.startSingleGameScrape(gameId);
}

// ── Emulator Settings ──────────────────────────────────────

QVariantList AppController::allEmulatorStatus() const {
    QVariantList list;
    for (const auto& emu : m_loader->allEmulators()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emu.id);
        QVariantMap item;
        item["id"] = emu.id;
        item["name"] = emu.name;
        item["description"] = emu.description;
        item["systems"] = emu.systems.join(", ");
        item["installed"] = adapter ? adapter->isInstalled(emu) : false;

        // BIOS summary
        bool biosRequired = false;
        bool biosDetected = false;
        if (adapter) {
            QString biosDir = Paths::biosDir();
            for (const auto& bios : adapter->biosFiles()) {
                if (bios.required) {
                    biosRequired = true;
                    if (QFileInfo::exists(biosDir + "/" + bios.filename)) {
                        biosDetected = true;
                    }
                }
            }
        }
        item["biosRequired"] = biosRequired;
        item["biosDetected"] = biosDetected;
        item["version"] = m_emuService.installedVersion(emu.id);

        list.append(item);
    }
    return list;
}

void AppController::installEmulator(const QString& emuId) {
    m_emuService.installEmulatorAsync(emuId);
}

void AppController::uninstallEmulator(const QString& emuId) {
    m_emuService.uninstallEmulator(emuId);
}

QVariantList AppController::biosStatus(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    QString biosDir = Paths::biosDir();
    for (const auto& bios : adapter->biosFiles()) {
        QVariantMap item;
        item["filename"] = bios.filename;
        item["description"] = bios.description;
        item["required"] = bios.required;
        item["found"] = QFileInfo::exists(biosDir + "/" + bios.filename);
        list.append(item);
    }
    return list;
}

void AppController::openBiosFolder() {
    QString biosDir = Paths::biosDir();
    QDir().mkpath(biosDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(biosDir));
}

void AppController::openRomFolder() {
    QString dir = Paths::romsDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void AppController::checkForUpdates() {
    m_emuService.checkForUpdates();
}

// ── Config Settings ────────────────────────────────────────

QVariantList AppController::settingsSchema(const QString& emuId) const { return m_configService.settingsSchema(emuId); }
QString AppController::settingValue(const QString& emuId, const QString& section, const QString& key) const { return m_configService.settingValue(emuId, section, key); }
void AppController::saveSettings(const QString& emuId, const QVariantMap& values) { m_configService.saveSettings(emuId, values); }
void AppController::beginSettingsSession(const QString& emuId) { m_configService.beginSettingsSession(emuId); }
void AppController::endSettingsSession(const QString& emuId) { m_configService.endSettingsSession(emuId); }
void AppController::resetConfiguration(const QString& emuId) { m_configService.resetConfiguration(emuId); }

QVariantList AppController::quickResolutionOptions(const QString& emuId) const { return m_configService.quickResolutionOptions(emuId); }
QString AppController::currentResolution(const QString& emuId) const { return m_configService.currentResolution(emuId); }
void AppController::applyQuickResolution(const QVariantMap& choices) { m_configService.applyQuickResolution(choices); }
QVariantList AppController::quickAspectRatioOptions(const QString& emuId) const { return m_configService.quickAspectRatioOptions(emuId); }
QString AppController::currentAspectRatio(const QString& emuId) const { return m_configService.currentAspectRatio(emuId); }
void AppController::applyQuickAspectRatio(const QVariantMap& choices) { m_configService.applyQuickAspectRatio(choices); }

QVariantList AppController::pathDefs(const QString& emuId) const { return m_configService.pathDefs(emuId); }
QString AppController::pathValue(const QString& emuId, const QString& section, const QString& key) const { return m_configService.pathValue(emuId, section, key); }
QString AppController::pathDefault(const QString& emuId, const QString& section, const QString& key) const { return m_configService.pathDefault(emuId, section, key); }
void AppController::savePaths(const QString& emuId, const QVariantMap& values) { m_configService.savePaths(emuId, values); }

QString AppController::browsePath(const QString& title) {
    return QFileDialog::getExistingDirectory(nullptr, title, QDir::homePath(), QFileDialog::ShowDirsOnly);
}

QString AppController::formatCapturedBinding(const QString& emuId, int deviceIndex, const QString& element, bool isAxis, bool positive) const {
    return m_configService.formatCapturedBinding(emuId, deviceIndex, element, isAxis, positive);
}
QString AppController::formatCapturedKeyboard(const QString& emuId, int qtKey, int modifiers) const { return m_configService.formatCapturedKeyboard(emuId, qtKey, modifiers); }
QString AppController::formatCapturedMouse(const QString& emuId, int qtButton) const { return m_configService.formatCapturedMouse(emuId, qtButton); }
QString AppController::formatCapturedWheel(const QString& emuId, int direction) const { return m_configService.formatCapturedWheel(emuId, direction); }

void AppController::showControllerMapping(const QString& emuId) {
    // Single-type emulators: pick the first/only controller type.
    QString typeId;
    const auto types = controllerTypes(emuId);
    if (!types.isEmpty()) typeId = types.first().toMap().value("id").toString();
    showControllerMapping(emuId, typeId);
}

void AppController::showControllerMapping(const QString& emuId,
                                           const QString& controllerTypeId) {
    if (!m_inputManager) {
        qWarning() << "[AppController] No SdlInputManager set";
        return;
    }
    auto* dialog = new ControllerMappingPage(m_inputManager, this, emuId,
                                              controllerTypeId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void AppController::showEmulatorSettings(const QString& emuId) {
    if (emuId == QLatin1String("pcsx2")) {
        auto* dialog = new Pcsx2SettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    if (emuId == QLatin1String("duckstation")) {
        auto* dialog = new DuckStationSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    if (emuId == QLatin1String("ppsspp")) {
        auto* dialog = new PpssppSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    if (emuId == QLatin1String("dolphin")) {
        auto* dialog = new DolphinSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    if (emuId == QLatin1String("mgba")) {
        auto* dialog = new MgbaSettingsDialog(this, emuId);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return;
    }
    qWarning() << "showEmulatorSettings: no settings dialog registered for emulator" << emuId;
}

void AppController::openNativeEmulatorSettings(const QString& emuId) {
    const auto* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return;

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString installPath = Paths::emulatorsDir(manifest->install_folder);
    QString exec = adapter->resolveExecutable(*manifest, installPath);
    if (exec.isEmpty() || !QFileInfo::exists(exec)) {
        setStatus(manifest->name + " is not installed.");
        return;
    }

    // Run the same setup that happens before launching a game so the native
    // UI sees a properly initialized config (wizard suppressed, paths set, etc.)
    const QString systemId = Paths::systemIdFor(emuId, manifest->systems);
    adapter->ensureConfig(*manifest, Paths::biosDir(),
                          Paths::emulatorDataDir(emuId, systemId));

#if defined(Q_OS_MACOS)
    // Defensive: re-strip the quarantine attribute. If it's re-applied (user
    // replaced the bundle, zip was re-extracted, etc.), macOS would translocate
    // the app to a read-only path, and atomic-save rename() would fail with
    // EPERM — crashing the emulator. Idempotent if already clean.
    QProcess::execute("xattr", {"-rd", "com.apple.quarantine", installPath});
#endif

    // Direct exec — bypassing Launch Services on macOS. Using `open` would
    // route through Launch Services which applies app translocation/sandbox
    // rules to downloaded .app bundles, causing emulators like DuckStation to
    // fail when trying to save settings inside their own bundle.
    //
    // additionalLaunchArgs() lets adapters inject CLI flags that must be
    // present on every launch (e.g. Dolphin's `-u <user-dir>`). Default empty.
    QProcess::startDetached(exec, adapter->additionalLaunchArgs());
}

void AppController::showHotkeySettings(const QString& emuId) {
    if (!m_inputManager) {
        qWarning() << "[AppController] No SdlInputManager set";
        return;
    }
    auto* dialog = new HotkeySettingsDialog(m_inputManager, this, emuId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void AppController::setCursorVisible(bool visible) {
    // QGuiApplication's override cursor is a STACK, not a state. If we
    // paired push/pop naively, rapid double-close during the settings
    // slide-out animation would push an extra BlankCursor that no
    // subsequent open() can pop, leaving the cursor stuck hidden. Track
    // the intended state explicitly and no-op when the caller asks for
    // what we already have. m_cursorVisible defaults to false to match
    // the startup BlankCursor push in main.cpp.
    if (visible == m_cursorVisible) return;
    m_cursorVisible = visible;
    if (visible) {
        QGuiApplication::restoreOverrideCursor();
    } else {
        QGuiApplication::setOverrideCursor(Qt::BlankCursor);
    }
}

QVariantList AppController::hotkeyBindings(const QString& emuId) const { return m_configService.hotkeyBindings(emuId); }
bool AppController::hasHotkeys(const QString& emuId) const { return !m_configService.hotkeyBindings(emuId).isEmpty(); }
void AppController::saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value) { m_configService.saveHotkey(emuId, section, key, value); }
void AppController::clearHotkey(const QString& emuId, const QString& section, const QString& key) { m_configService.clearHotkey(emuId, section, key); }
void AppController::resetHotkeys(const QString& emuId) { m_configService.resetHotkeys(emuId); }

QVariantList AppController::controllerTypes(const QString& emuId) const { return m_configService.controllerTypes(emuId); }
QString AppController::controllerType(const QString& emuId, int port) const { return m_configService.controllerType(emuId, port); }
QVariantList AppController::controllerBindingsForPort(const QString& emuId, int port,
                                                       const QString& controllerTypeId) const {
    return m_configService.controllerBindingsForPort(emuId, port, controllerTypeId);
}
void AppController::saveBindingForPort(const QString& emuId, int port,
                                        const QString& controllerTypeId,
                                        const QString& key, const QString& value,
                                        int deviceIndex) {
    m_configService.saveBindingForPort(emuId, port, controllerTypeId, key, value, deviceIndex);
}
void AppController::clearBindingForPort(const QString& emuId, int port,
                                         const QString& controllerTypeId,
                                         const QString& key) {
    m_configService.clearBindingForPort(emuId, port, controllerTypeId, key);
}
void AppController::clearAllBindingsForPort(const QString& emuId, int port,
                                             const QString& controllerTypeId) {
    m_configService.clearAllBindingsForPort(emuId, port, controllerTypeId);
}
void AppController::autoMapControllerForPort(const QString& emuId, int port,
                                              const QString& controllerTypeId,
                                              int deviceIndex) {
    m_configService.autoMapControllerForPort(emuId, port, controllerTypeId, deviceIndex);
}

// ── Scraper ───────────────────────────────────────────────

void AppController::validateScraperCredentials(const QString& user, const QString& pass) {
    setStatus("Validating credentials...");
    // validateAndSaveCredentials runs on a worker thread; the result is
    // delivered via ScraperService::credentialsValidated, which the ctor
    // connects to status + scraperCredentialsValidated emission.
    m_scraperService.validateAndSaveCredentials(user, pass);
}

void AppController::scraperSignOut() {
    m_scraperService.signOut();
    emit scraperSignedOut();
    setStatus("Signed out of ScreenScraper.");
}

bool AppController::hasScraperCredentials() const {
    return m_scraperService.hasCredentials();
}

QString AppController::scraperUsername() const {
    return m_scraperService.credentials().ssId;
}

void AppController::startBatchScrape(const QStringList& mediaTypes,
                                      const QStringList& systems,
                                      const QString& gameFilter) {
    ScraperService::ScrapeOptions options;
    options.mediaTypes = mediaTypes;
    options.systems = systems;

    if (gameFilter == "unscraped")
        options.gameFilter = ScraperService::ScrapeOptions::UnscrapedOnly;
    else if (gameFilter == "favorites")
        options.gameFilter = ScraperService::ScrapeOptions::FavoritesOnly;
    else
        options.gameFilter = ScraperService::ScrapeOptions::AllGames;

    m_scraperService.startBatchScrape(options);
}

void AppController::cancelScrape() {
    m_scraperService.cancelScrape();
    setStatus("Scrape cancelled.");
}

QVariantList AppController::scrapableSystems() const {
    QVariantList list;
    auto counts = m_db->systemGameCounts();
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        QVariantMap item;
        item["id"] = it.key();
        item["name"] = it.key();
        item["count"] = it.value();
        list.append(item);
    }
    return list;
}

QStringList AppController::allMediaTypes() const {
    return Scraper::allMediaTypes();
}

int AppController::scrapeGameCount(const QStringList& systems, const QString& gameFilter) const {
    int count = 0;
    for (const auto& system : systems) {
        auto games = m_db->gamesBySystem(system);
        for (const auto& g : games) {
            if (gameFilter == "unscraped") {
                if (g.cover_path.isEmpty() || !QFileInfo::exists(g.cover_path))
                    count++;
            } else if (gameFilter == "favorites") {
                if (g.favorite)
                    count++;
            } else {
                count++;
            }
        }
    }
    return count;
}

// ── RetroAchievements ──────────────────────────────────────────

void AppController::raLogin(const QString& username, const QString& apiKey) {
    m_raService.login(username, apiKey);
}

QVariantMap AppController::currentGameInfo() const {
    QString romPath = m_gameService.currentRomPath();
    if (romPath.isEmpty()) return {};
    auto games = m_db->allGames();
    for (const auto& game : games) {
        if (game.rom_path == romPath) {
            QVariantMap info;
            info["title"] = game.title;
            info["system"] = game.system;
            info["gameId"] = game.id;
            info["emuId"] = game.emulator_id;
            auto* adapter = AdapterRegistry::instance().adapterFor(game.emulator_id);
            info["supportsSaveOnExit"] = adapter ? adapter->supportsSaveOnExit() : false;
            // External-emulator path uses synthesized hotkeys; libretro
            // path uses direct CoreRuntime calls. Libretro cores always
            // support save/load state via retro_serialize/unserialize and
            // fast-forward via setSpeedMultiplier — no per-core opt-in
            // needed.
            const bool isLibretro = dynamic_cast<LibretroAdapter*>(adapter) != nullptr;
            const bool synthSave = adapter
                && adapter->hotkeyVirtualKeyCode(EmulatorAdapter::HotkeyAction::SaveState) != 0;
            const bool synthLoad = adapter
                && adapter->hotkeyVirtualKeyCode(EmulatorAdapter::HotkeyAction::LoadState) != 0;
            const bool synthFf = adapter
                && adapter->hotkeyVirtualKeyCode(EmulatorAdapter::HotkeyAction::ToggleFastForward) != 0;
            info["supportsSaveState"] = isLibretro || synthSave;
            info["supportsLoadState"] = isLibretro || synthLoad;
            info["supportsFastForward"] = isLibretro || synthFf;
            return info;
        }
    }
    return {};
}

void AppController::raSignOut() {
    m_raService.signOut();
    // Disable RA in emulator configs
    for (const auto& manifest : m_loader->allEmulators()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(manifest.id);
        if (adapter && adapter->supportsRetroAchievements()) {
            adapter->patchRetroAchievements("", "", false, false, false, false);
        }
    }
}

bool AppController::hasRACredentials() const { return m_raService.hasCredentials(); }
QString AppController::raUsername() const { return m_raService.username(); }
void AppController::raRequestUserSummary() { m_raService.requestUserSummary(); }
void AppController::raRequestUserGames() { m_raService.requestUserGames(); }
void AppController::raRequestGameDetail(int raGameId) { m_raService.requestGameDetail(raGameId); }
void AppController::raRequestGameIdLookup(const QString& title, const QString& system) {
    m_raService.requestGameIdLookup(title, system);
}

void AppController::raProceedAfterLoginPrompt() {
    if (m_pendingLaunchRom.isEmpty()) return;
    QString rom = m_pendingLaunchRom;
    QString emu = m_pendingLaunchEmu;
    QStringList args = m_pendingLaunchArgs;
    m_pendingLaunchRom.clear();
    m_pendingLaunchEmu.clear();
    m_pendingLaunchArgs.clear();
    // Re-call launchGame — prompt won't show again (already marked)
    launchGame(0, rom, emu, args);
}
void AppController::raLoginWithPassword(const QString& username, const QString& password) {
    m_raService.loginWithPassword(username, password);
}

bool AppController::raHasLibretroToken() const { return m_raService.hasLibretroToken(); }

bool AppController::raHardcoreMode() const { return m_raService.hardcoreMode(); }
void AppController::raSetHardcoreMode(bool enabled) {
    m_raService.setHardcoreMode(enabled);
    // Mid-session honor: same pattern as raSetEncoreMode. If a libretro
    // game is running, push the new value into the live rc_client so
    // hardcore takes effect (or releases) without a relaunch. rcheevos
    // raises a RC_CLIENT_EVENT_RESET when hardcore flips on with a game
    // already loaded — our event handler invokes retro_reset to wipe
    // any save-state contamination, satisfying RA's hardcore rule.
    auto* sess = m_gameService.session();
    if (sess && sess->isRunning() && sess->isLibretro()) {
        if (auto* lr = dynamic_cast<LibretroAdapter*>(sess->adapter()))
            if (auto* rt = lr->runtime())
                rt->rcheevos().setHardcore(enabled);
    }
}
bool AppController::raNotifications() const { return m_raService.notifications(); }
void AppController::raSetNotifications(bool enabled) { m_raService.setNotifications(enabled); }
bool AppController::raSoundEffects() const { return m_raService.soundEffects(); }
void AppController::raSetSoundEffects(bool enabled) { m_raService.setSoundEffects(enabled); }

bool AppController::gameUsesHardwareRender() {
    auto* session = m_gameService.session();
    if (!session || !session->isLibretro()) return false;
    auto* libretro = dynamic_cast<LibretroAdapter*>(session->adapter());
    return libretro && libretro->prefersHardwareRender();
}

bool AppController::raEncoreMode() const { return m_raService.encoreMode(); }
void AppController::raSetEncoreMode(bool enabled) {
    m_raService.setEncoreMode(enabled);
    // Mid-session honor: if a libretro game is running, push the new
    // value into the live rc_client so encore takes effect without
    // requiring a relaunch. The persistent setting also gets picked
    // up at next session start via LibretroRaConfig.
    auto* sess = m_gameService.session();
    if (sess && sess->isRunning() && sess->isLibretro()) {
        if (auto* lr = dynamic_cast<LibretroAdapter*>(sess->adapter()))
            if (auto* rt = lr->runtime())
                rt->rcheevos().setEncore(enabled);
    }
}

bool AppController::libretroAchievementsReady() {
    auto* sess = m_gameService.session();
    if (!sess || !sess->isRunning() || !sess->isLibretro()) return false;
    auto* lr = dynamic_cast<LibretroAdapter*>(sess->adapter());
    if (!lr || !lr->runtime()) return false;
    return lr->runtime()->rcheevos().isInSession();
}

QVariantList AppController::libretroAchievementList() {
    if (!libretroAchievementsReady()) return {};
    auto* sess = m_gameService.session();
    auto* lr = dynamic_cast<LibretroAdapter*>(sess->adapter());
    return lr->runtime()->rcheevos().achievementListVariants();
}

void AppController::setSdlInputManager(SdlInputManager* mgr) {
    m_inputManager = mgr;
    m_gameService.session()->setSdlInputManager(mgr);
    m_configService.setSdlInputManager(mgr);
}

void AppController::setQmlEngine(QQmlEngine* engine) {
    m_qmlEngine = engine;
}

namespace {
using HotkeyAction = EmulatorAdapter::HotkeyAction;

// Ask the running adapter for the kVK_* it has bound to the given
// action and synthesize it to the emulator process. Returns true if
// a key was sent. The caller decides what to do when no key is
// available — pause/resume falls back to SIGSTOP/SIGCONT, the
// in-game menu actions just no-op.
//
// Ask the running adapter for the kVK_* it has bound to the given
// action and synthesize it to the emulator process. Returns true if
// a key was sent. Pause/resume falls back to SIGSTOP/SIGCONT when no
// key is available; in-game menu actions just no-op.
bool synthesizeHotkey(GameSession* sess, int64_t pid, HotkeyAction action) {
    if (!sess) return false;
    auto* adapter = sess->adapter();
    const int vk = adapter ? adapter->hotkeyVirtualKeyCode(action) : 0;
    if (vk == 0) return false;
    MacFullscreen::sendKeyToProcess(pid, vk);
    return true;
}

void emulatorPause(GameSession* sess, int64_t pid) {
    if (!synthesizeHotkey(sess, pid, HotkeyAction::TogglePause))
        MacFullscreen::pauseProcess(pid);
}

void emulatorResume(GameSession* sess, int64_t pid) {
    if (!synthesizeHotkey(sess, pid, HotkeyAction::TogglePause))
        MacFullscreen::resumeProcess(pid);
}
} // namespace

void AppController::openInGameMenuPanel() {
    qDebug() << "[InGameMenuPanel] open requested";
    if (!m_qmlEngine) {
        qWarning() << "[AppController] openInGameMenuPanel before QML engine set";
        return;
    }
    if (!m_inGameMenuPanel) {
        m_inGameMenuPanel = new InGameMenuPanel(m_qmlEngine, this);
        connect(m_inGameMenuPanel, &InGameMenuPanel::resumeRequested,
                this, [this]() {
                    closeInGameMenuPanel();
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::exitWithSaveRequested,
                this, [this]() {
                    // Unpause the emulator first so its save thread
                    // can run on the SIGTERM that saveAndStopGame
                    // will send.
                    m_inGameMenuPanel->hide();
                    if (m_inputManager) m_inputManager->setSuppressMainInputs(false);
                    if (auto* sess = gameSession()) {
                        const int64_t pid = sess->pid();
                        if (pid > 0 && m_emulatorSuspended) {
                            emulatorResume(sess, pid);
                            m_emulatorSuspended = false;
                        }
                    }
                    saveAndStopGame(1);
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::exitWithoutSaveRequested,
                this, [this]() {
                    m_inGameMenuPanel->hide();
                    if (m_inputManager) m_inputManager->setSuppressMainInputs(false);
                    if (auto* sess = gameSession()) {
                        const int64_t pid = sess->pid();
                        if (pid > 0 && m_emulatorSuspended) {
                            emulatorResume(sess, pid);
                            m_emulatorSuspended = false;
                        }
                    }
                    stopGame();
                });
        // Save / Load / Fast-forward: synth the action key, then close
        // the menu (which handles activate + button-release poll +
        // unpause). The synth path is per-adapter — see
        // synthesizeHotkey: PidEvents goes via CGEventPostToPid (lands
        // in the emulator's NSEvent queue immediately), HidTap goes
        // via AppleScript / System Events (activates the process and
        // dispatches the keystroke through Apple Events).
        auto synthThenClose = [this](EmulatorAdapter::HotkeyAction action) {
            if (auto* sess = gameSession()) {
                const int64_t pid = sess->pid();
                if (pid > 0) synthesizeHotkey(sess, pid, action);
            }
            closeInGameMenuPanel();
        };
        connect(m_inGameMenuPanel, &InGameMenuPanel::saveStateRequested,
                this, [synthThenClose]() {
                    synthThenClose(EmulatorAdapter::HotkeyAction::SaveState);
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::loadStateRequested,
                this, [synthThenClose]() {
                    synthThenClose(EmulatorAdapter::HotkeyAction::LoadState);
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::toggleFastForwardRequested,
                this, [synthThenClose]() {
                    synthThenClose(EmulatorAdapter::HotkeyAction::ToggleFastForward);
                });
        connect(m_inGameMenuPanel, &InGameMenuPanel::visibilityChanged,
                this, &AppController::inGameMenuPanelVisibleChanged);
    }

    int64_t pid = 0;
    GameSession* sess = gameSession();
    if (sess) pid = sess->pid();
    if (pid > 0 && !m_emulatorSuspended) {
        emulatorPause(sess, pid);
        m_emulatorSuspended = true;
    }
    // Tell SDL to suppress main-window signal emits (e.g. Start
    // pressing in the menu shouldn't open the main app's settings
    // overlay). Key injection still flows to the focused window
    // (the panel), so HUD navigation continues to work.
    if (m_inputManager) m_inputManager->setSuppressMainInputs(true);
    m_inGameMenuPanel->showOverEmulator(pid);
}

void AppController::closeInGameMenuPanel() {
    if (!m_inGameMenuPanel) return;
    m_inGameMenuPanel->hide();
    if (m_inputManager) m_inputManager->setSuppressMainInputs(false);

    // If a previous close is still polling, tear it down. Captures
    // are held by std::shared_ptr below, so disconnecting + dropping
    // the lambda releases them deterministically (no leak even if the
    // user fast-toggles the menu).
    if (m_resumeWhenButtonsReleasedTimer) {
        m_resumeWhenButtonsReleasedTimer->stop();
        m_resumeWhenButtonsReleasedTimer->disconnect();
    }

    GameSession* sess = gameSession();
    int64_t pid = sess ? sess->pid() : 0;
    if (pid <= 0 || !m_emulatorSuspended) {
        m_emulatorSuspended = false;
        return;
    }
    MacFullscreen::activateProcess(pid);

    // Poll SDL state at 16 ms; resume only once all action buttons
    // (A/B/X/Y) are released. Variable delay (~50–150 ms typical) so
    // the close-trigger button can never leak as in-game input.
    // Safety timeout at 500 ms (~30 ticks).
    if (!m_resumeWhenButtonsReleasedTimer) {
        m_resumeWhenButtonsReleasedTimer = new QTimer(this);
        m_resumeWhenButtonsReleasedTimer->setInterval(16);
    }
    auto tickCount = std::make_shared<int>(0);
    connect(m_resumeWhenButtonsReleasedTimer, &QTimer::timeout, this,
        [this, sess, pid, tickCount]() {
            const bool buttonsHeld =
                m_inputManager && m_inputManager->isAnyActionButtonPressed();
            if (!buttonsHeld || ++(*tickCount) > 30) {
                if (m_emulatorSuspended) {
                    emulatorResume(sess, pid);
                    m_emulatorSuspended = false;
                }
                m_resumeWhenButtonsReleasedTimer->stop();
                m_resumeWhenButtonsReleasedTimer->disconnect();
            }
        });
    m_resumeWhenButtonsReleasedTimer->start();
}

bool AppController::inGameMenuPanelVisible() const {
    return m_inGameMenuPanel && m_inGameMenuPanel->isVisible();
}
