#include "app_controller.h"
#include "adapters/adapter_registry.h"
#include "core/ini_file.h"
#include "core/macos_fullscreen.h"
#include "core/paths.h"
#include "core/scraper.h"
#include "core/setting_def.h"

#include <QCursor>
#include "settings/controller_mapping_page.h"
#include "settings/pcsx2/pcsx2_settings_dialog.h"
#include "settings/duckstation/duckstation_settings_dialog.h"
#include "settings/ppsspp/ppsspp_settings_dialog.h"
#include "settings/hotkey_settings_page.h"
#include "core/sdl_input_manager.h"

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

    connect(&m_gameService, &GameService::statusMessage, this, &AppController::setStatus);
    connect(&m_gameService, &GameService::gameRunningChanged, this, &AppController::gameRunningChanged);
    connect(&m_gameService, &GameService::gameStarted, this, &AppController::gameStarted);
    connect(&m_gameService, &GameService::gameFinished, this, &AppController::gameFinished);
    // Register global Cmd+Escape hotkey and wire to signal
    static AppController* s_instance = nullptr;
    s_instance = this;
    MacFullscreen::registerGlobalHotkey([]() {
        if (s_instance)
            emit s_instance->globalHotkeyPressed();
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
    connect(&m_raService, &RAService::userSummaryReady, this, &AppController::raUserSummaryReady);
    connect(&m_raService, &RAService::userGamesReady, this, &AppController::raUserGamesReady);
    connect(&m_raService, &RAService::gameDetailReady, this, &AppController::raGameDetailReady);
    connect(&m_raService, &RAService::gameIdLookupReady, this, &AppController::raGameIdLookupReady);
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

void AppController::activateEmulator() {
    auto* session = m_gameService.session();
    if (session && session->isRunning())
        MacFullscreen::activateProcess(session->pid());
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
    if (!m_inputManager) {
        qWarning() << "[AppController] No SdlInputManager set";
        return;
    }
    auto* dialog = new ControllerMappingPage(m_inputManager, this, emuId);
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
    QProcess::startDetached(exec, {});
}

void AppController::showHotkeySettings(const QString& emuId) {
    if (!m_inputManager) {
        qWarning() << "[AppController] No SdlInputManager set";
        return;
    }
    auto* dialog = new HotkeySettingsPage(m_inputManager, this, emuId);
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
void AppController::saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value) { m_configService.saveHotkey(emuId, section, key, value); }
void AppController::clearHotkey(const QString& emuId, const QString& section, const QString& key) { m_configService.clearHotkey(emuId, section, key); }
void AppController::resetHotkeys(const QString& emuId) { m_configService.resetHotkeys(emuId); }

QVariantList AppController::controllerTypes(const QString& emuId) const { return m_configService.controllerTypes(emuId); }
QString AppController::controllerType(const QString& emuId, int port) const { return m_configService.controllerType(emuId, port); }
void AppController::setControllerType(const QString& emuId, int port, const QString& type) { m_configService.setControllerType(emuId, port, type); }

QVariantList AppController::controllerBindingsForPort(const QString& emuId, int port) const { return m_configService.controllerBindingsForPort(emuId, port); }
QVariantList AppController::controllerSettingsForPort(const QString& emuId, int port) const { return m_configService.controllerSettingsForPort(emuId, port); }
void AppController::saveBindingForPort(const QString& emuId, int port, const QString& key, const QString& value) { m_configService.saveBindingForPort(emuId, port, key, value); }
void AppController::clearBindingForPort(const QString& emuId, int port, const QString& key) { m_configService.clearBindingForPort(emuId, port, key); }
void AppController::clearAllBindingsForPort(const QString& emuId, int port) { m_configService.clearAllBindingsForPort(emuId, port); }
void AppController::autoMapControllerForPort(const QString& emuId, int port, int deviceIndex) { m_configService.autoMapControllerForPort(emuId, port, deviceIndex); }
void AppController::saveControllerSettingForPort(const QString& emuId, int port, const QString& key, const QString& value) { m_configService.saveControllerSettingForPort(emuId, port, key, value); }
void AppController::restoreDefaultsForPort(const QString& emuId, int port) { m_configService.restoreDefaultsForPort(emuId, port); }

QStringList AppController::controllerProfiles(const QString& emuId) const { return m_configService.controllerProfiles(emuId); }
void AppController::createControllerProfile(const QString& emuId, const QString& name) { m_configService.createControllerProfile(emuId, name); }
void AppController::applyControllerProfile(const QString& emuId, const QString& name) { m_configService.applyControllerProfile(emuId, name); }
void AppController::renameControllerProfile(const QString& emuId, const QString& oldName, const QString& newName) { m_configService.renameControllerProfile(emuId, oldName, newName); }
void AppController::deleteControllerProfile(const QString& emuId, const QString& name) { m_configService.deleteControllerProfile(emuId, name); }

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
bool AppController::raHardcoreMode() const { return m_raService.hardcoreMode(); }
void AppController::raSetHardcoreMode(bool enabled) { m_raService.setHardcoreMode(enabled); }
bool AppController::raNotifications() const { return m_raService.notifications(); }
void AppController::raSetNotifications(bool enabled) { m_raService.setNotifications(enabled); }
bool AppController::raSoundEffects() const { return m_raService.soundEffects(); }
void AppController::raSetSoundEffects(bool enabled) { m_raService.setSoundEffects(enabled); }
