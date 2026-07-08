#include "app_controller.h"
#include "services/patches_installer.h"
#include "adapters/adapter_registry.h"
#include "adapters/libretro/libretro_adapter.h"
#include "core/detail_actions.h"
#include "core/macos_fullscreen.h"
#include "core/paths.h"
#include "core/scraper.h"
#include "in_game_menu_controller.h"
#include "core/sdl_input_manager.h"
#include "core/libretro/libretro_hotkey_defs.h"
#include "core/libretro/libretro_hotkey_controller.h"
#include <QCoreApplication>
#include <QApplication>
#include <QKeyEvent>

#include <QQmlEngine>
#include <QTimer>
#include <QPointer>

#include <QCursor>
#include "settings/controller_mapping_page.h"
#include "settings/generic_emulator_settings_dialog.h"
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
    , m_hotkeyService(this)
    , m_configService(loader, &m_hotkeyService)
{
    connect(&m_configService, &ConfigService::statusMessage, this, &AppController::setStatus);
    connect(&m_configService, &ConfigService::configurationReset,
            this, &AppController::configurationReset);
    connect(&m_hotkeyService, &HotkeyService::statusMessage, this, &AppController::setStatus);
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
    // Pattern B HW-render libretro cores need a transparent QQuickWindow
    // floating above the Metal NSView to host the in-game menu, RA toasts,
    // RA badge, and indicator bar — without it the game NSView composites
    // on top of any in-scene QML overlay. InGameMenuController owns that
    // overlay; show it pre-launch so the scene is realised before the
    // first frame, hide it on game end.
    connect(this, &AppController::gameStartingLibretro, this, [this] {
        if (gameUsesHardwareRender() && m_inGameMenu)
            m_inGameMenu->showLibretroOverlayForCurrentGame();
    });
    connect(&m_gameService, &GameService::gameFinished, this, [this](int, bool) {
        if (m_inGameMenu) m_inGameMenu->hideLibretroOverlay();
    });
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
            if (success) emit emulatorStatusChanged(emuId);
            emit installFinished(emuId, success, message);
        });
    connect(&m_emuService, &EmulatorService::uninstallFinished, this,
        [this](const QString& emuId, bool success, const QString& message) {
            setStatus(message);
            if (success) emit emulatorStatusChanged(emuId);
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
            this, &AppController::infoToast);
    connect(&m_raService, &RAService::indicator,
            this, &AppController::raIndicator);

    // ── Libretro global hotkeys ───────────────────────────────
    // The whole engine (matcher + dispatcher + qApp key filter +
    // suppression + binding sync) lives in LibretroHotkeyController;
    // AppController supplies the session provider + the Qt-modal-widget
    // suppression check, and forwards the controller's signals to QML.
    m_libretroHotkeys = std::make_unique<LibretroHotkeyController>(
        [this]() -> GameSession* { return m_gameService.session(); },
        []() { return QApplication::activeModalWidget() != nullptr; });
    connect(m_libretroHotkeys.get(), &LibretroHotkeyController::menuToggleRequested,
            // Dedicated signal (NOT globalHotkeyPressed, the Carbon
            // Cmd+Shift+Esc hotkey). QML handles it by calling
            // toggleInGameMenu, which selects the libretro-aware menu
            // (overlay / in-scene HUD).
            this, &AppController::libretroMenuToggleRequested);
    connect(m_libretroHotkeys.get(), &LibretroHotkeyController::infoToastRequested,
            this, &AppController::infoToast);

    // Explicit injection of the matcher for the worker-thread combo-
    // modifier suppression lookup (GameSession -> CoreRuntime). Cleared
    // in ~AppController.
    m_gameService.session()->setHotkeyMatcher(m_libretroHotkeys->matcher());

    // SDL stops emitting gamepadButtonChanged into the matcher while an
    // in-game menu / overlay is open (m_emulationTarget is cleared). The
    // matcher's cached held-button set would otherwise go stale and the
    // next press-edge wouldn't fire. Reset the gamepad state whenever the
    // menu closes so the next press starts fresh. Single edge now —
    // InGameMenuController routes both backends through one signal.
    connect(this, &AppController::inGameMenuOpenChanged, this, [this]{
        if (!inGameMenuOpen())
            m_libretroHotkeys->resetGamepadState();
    });

    syncLibretroHotkeyBindings();
}

AppController::~AppController() {
    // Clear the injected matcher before the controller (and its matcher)
    // are destroyed — GameSession forwards the nullptr to a live runtime.
    m_gameService.session()->setHotkeyMatcher(nullptr);
}

// ── Installer plumbing ─────────────────────────────────────

void AppController::attachPatchesInstaller(PatchesInstaller* installer) {
    m_patchesInstaller = installer;
    if (!m_patchesInstaller) return;
    connect(m_patchesInstaller, &PatchesInstaller::finished, this,
            [this](bool success, const QString& message, const QString& /*tag*/) {
                const bool manual = m_patchesManualRefresh;
                m_patchesManualRefresh = false;
                // Startup path: success-only toast (don't nag offline users).
                // Manual path: both success and failure toast (user asked).
                if (manual || success) emitPatchesToast(success, message);
            });
}

void AppController::refreshPcsx2Patches() {
    if (!m_patchesInstaller) {
        emitPatchesToast(false, "Patches installer not available");
        return;
    }
    m_patchesManualRefresh = true;
    m_patchesInstaller->fetchAsync(Paths::pcsx2ResourcesDir(), /*force*/ true);
}

void AppController::emitPatchesToast(bool success, const QString& message) {
    emit infoToast(
        /*header*/      "PCSX2 Patches",
        /*title*/       success ? "Updated" : "Update failed",
        /*description*/ message,
        /*imageUrl*/    QString(),
        /*durationMs*/  success ? 3500 : 5000);
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

void AppController::launchGame(int gameId, const QString& romPath, const QString& emuId) {
    // Check if we need to show a one-time RA login prompt BEFORE launching
    if (m_raService.hasCredentials()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (adapter && adapter->supportsRetroAchievements()) {
            if (m_raService.needsEmulatorLoginPrompt(emuId)) {
                auto* manifest = m_loader->emulatorById(emuId);
                QString name = (manifest && !manifest->name.isEmpty()) ? manifest->name : emuId;
                // Store pending launch info and show prompt — QML will call launchGame again after dismissal
                m_pendingLaunchGameId = gameId;
                m_pendingLaunchRom = romPath;
                m_pendingLaunchEmu = emuId;
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

    if (!m_gameService.startGame(gameId, romPath, emuId)) {
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

void AppController::removeGame(int gameId, bool deleteRomFile) {
    m_gameService.removeGame(gameId, deleteRomFile);
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

        // Packet 7 stage 3: manifest-driven UI capabilities.
        item["logo"] = emu.logo;
        item["detailActions"] = detailActionRows(emu, item["installed"].toBool(),
                                                 hasHotkeys(emu.id));

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
void AppController::resetPaths(const QString& emuId) { m_configService.resetPaths(emuId); }

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
    // Single factory path — the dialog reads its hub layout and category
    // schema from the registered adapter, so every emulator with a
    // settingsHubCards() override gets a settings UI for free. Replaces
    // the previous per-emulator if-chain + 5 dialog subclasses.
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) {
        qWarning() << "[AppController] showEmulatorSettings: no adapter for" << emuId;
        return;
    }
    if (adapter->settingsHubCards().isEmpty()) {
        qWarning() << "[AppController] showEmulatorSettings: adapter for" << emuId
                   << "exposes no settings hub cards — skipping";
        return;
    }
    const auto* manifest = m_loader ? m_loader->emulatorById(emuId) : nullptr;
    const QString displayName = manifest ? manifest->name : emuId;

    auto* dialog = new GenericEmulatorSettingsDialog(this, emuId, displayName,
                                                     adapter);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
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

void AppController::showLibretroHotkeySettings() {
    // Don't write app.libretroHotkeysSuppressed from C++ — the QML
    // modal-policy Binding owns it, and an imperative write would break
    // the Binding (Qt 6 semantics), leaving suppression stuck on the
    // next dialog open. The dialog doesn't need it anyway: it holds its
    // own refcount on LibretroHotkeyController for its whole lifetime
    // (see HotkeySettingsDialog's constructor).
    showHotkeySettings(libretro_hotkeys::kSentinelEmuId);
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

QString AppController::emulatorLogo(const QString& emuId) const {
    const EmulatorManifest* m = m_loader->emulatorById(emuId);
    return m ? m->logo : QString();
}

QVariantList AppController::hotkeyBindings(const QString& emuId) const { return m_hotkeyService.hotkeyBindings(emuId); }
bool AppController::hasHotkeys(const QString& emuId) const {
    // Libretro adapters use the app-wide Libretro Hotkeys settings page;
    // the per-emulator hotkey button is hidden for them.
    if (auto* a = AdapterRegistry::instance().adapterFor(emuId); a && a->asLibretro())
        return false;
    return !m_hotkeyService.hotkeyBindings(emuId).isEmpty();
}
void AppController::saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value) {
    m_hotkeyService.saveHotkey(emuId, section, key, value);
    if (emuId == libretro_hotkeys::kSentinelEmuId) syncLibretroHotkeyBindings();
}
void AppController::clearHotkey(const QString& emuId, const QString& section, const QString& key) {
    m_hotkeyService.clearHotkey(emuId, section, key);
    if (emuId == libretro_hotkeys::kSentinelEmuId) syncLibretroHotkeyBindings();
}
void AppController::resetHotkeys(const QString& emuId) {
    m_hotkeyService.resetHotkeys(emuId);
    if (emuId == libretro_hotkeys::kSentinelEmuId) syncLibretroHotkeyBindings();
}

void AppController::setLibretroHotkeysSuppressed(bool suppressed) {
    if (suppressed == m_libretroHotkeysSuppressed) return;
    m_libretroHotkeysSuppressed = suppressed;
    m_libretroHotkeys->setUiSuppressed(suppressed);
    emit libretroHotkeysSuppressedChanged();
}

void AppController::syncLibretroHotkeyBindings() {
    m_libretroHotkeys->setBindings(hotkeyBindings(libretro_hotkeys::kSentinelEmuId));
}

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

    options.gameFilter = ScraperService::ScrapeOptions::filterFromString(gameFilter);

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
    // Shares ScraperService's membership predicate so this confirmation
    // count can't drift from the set startBatchScrape actually scrapes.
    const auto filter = ScraperService::ScrapeOptions::filterFromString(gameFilter);
    int count = 0;
    for (const auto& system : systems) {
        const auto games = m_db->gamesBySystem(system);
        for (const auto& g : games) {
            if (ScraperService::matchesFilter(g, filter))
                count++;
        }
    }
    return count;
}

// ── RetroAchievements ──────────────────────────────────────────

void AppController::raLogin(const QString& username, const QString& apiKey) {
    m_raService.login(username, apiKey);
}

QVariantMap AppController::currentGameInfo() const {
    // Built from the record GameService cached at launch — no DB scan
    // per in-game-menu open (review P6).
    return m_gameService.currentGameInfo();
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
    int gameId  = m_pendingLaunchGameId;
    QString rom = m_pendingLaunchRom;
    QString emu = m_pendingLaunchEmu;
    m_pendingLaunchGameId = 0;
    m_pendingLaunchRom.clear();
    m_pendingLaunchEmu.clear();
    // Re-call launchGame — prompt won't show again (already marked)
    launchGame(gameId, rom, emu);
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
    if (auto* rt = m_gameService.libretroRuntime())
        rt->rcheevos().setHardcore(enabled);
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
    if (auto* rt = m_gameService.libretroRuntime())
        rt->rcheevos().setEncore(enabled);
}

bool AppController::libretroAchievementsReady() {
    auto* rt = m_gameService.libretroRuntime();
    return rt && rt->rcheevos().isInSession();
}

QVariantList AppController::libretroAchievementList() {
    auto* rt = m_gameService.libretroRuntime();
    if (!rt || !rt->rcheevos().isInSession()) return {};
    return rt->rcheevos().achievementListVariants();
}

void AppController::setSdlInputManager(SdlInputManager* mgr) {
    m_inputManager = mgr;
    m_gameService.session()->setSdlInputManager(mgr);
    m_configService.setSdlInputManager(mgr);
    // Route SDL gamepad edges into the libretro hotkey matcher.
    // SdlInputManager only emits gamepadButtonChanged while emulation mode
    // is active (i.e. a libretro game is running) — see the m_emulationTarget
    // branch in pollEvents() — so this connection is a no-op outside libretro
    // sessions without needing a runtime gate here.
    m_libretroHotkeys->attachInputManager(mgr);
}

void AppController::setQmlEngine(QQmlEngine* engine) {
    m_qmlEngine = engine;

    // Create the in-game menu controller now that we have the engine.
    // Action signals fire from the overlay panel; the policy of what each
    // action does (GameSession calls) lives here.
    m_inGameMenu = new InGameMenuController(m_qmlEngine, this);

    // The pause/resume invariant lives on the menu-open EDGE inside the
    // controller (see InGameMenuController::setPauseHook) — none of the
    // action handlers below manage it anymore. closeMenu() synchronously
    // fires the edge, so exits resume the core before stop runs.
    m_inGameMenu->setPauseHook([this](bool paused) {
        if (auto* s = gameSession()) {
            if (paused) s->pauseEmulation();
            else        s->resumeEmulation();
        }
    });

    connect(m_inGameMenu, &InGameMenuController::menuOpenChanged,
            this, &AppController::inGameMenuOpenChanged);

    connect(m_inGameMenu, &InGameMenuController::resumeRequested,
            this, [this] { closeInGameMenu(); });
    connect(m_inGameMenu, &InGameMenuController::exitWithSaveRequested,
            this, [this] {
                m_inGameMenu->closeMenu();
                saveAndStopGame(1);
            });
    connect(m_inGameMenu, &InGameMenuController::exitWithoutSaveRequested,
            this, [this] {
                m_inGameMenu->closeMenu();
                stopGame();
            });
    connect(m_inGameMenu, &InGameMenuController::saveStateRequested,
            this, [this] {
                if (auto* s = gameSession())
                    s->saveStateLibretro(s->currentSaveSlot());
                m_inGameMenu->closeMenu();
            });
    connect(m_inGameMenu, &InGameMenuController::loadStateRequested,
            this, [this] {
                if (auto* s = gameSession())
                    s->loadStateLibretro(s->currentSaveSlot());
                m_inGameMenu->closeMenu();
            });
    connect(m_inGameMenu, &InGameMenuController::toggleFastForwardRequested,
            this, [this] {
                if (auto* s = gameSession()) s->toggleFastForwardLibretro();
                // Leave the menu open — FF is a state toggle.
                // QML LibretroOverlayPanel listens to
                // GameSession::libretroFastForwardChanged and shows/
                // hides its ffToast pill from that signal.
            });
}

void AppController::openInGameMenu() {
    qDebug() << "[InGameMenu] open requested";
    if (!m_inGameMenu) {
        qWarning() << "[AppController] openInGameMenu before QML engine set";
        return;
    }
    // Pause happens on the menu-open edge via the controller's pause
    // hook — see setQmlEngine.
    m_inGameMenu->openMenu();
}

void AppController::closeInGameMenu() {
    if (!m_inGameMenu) return;
    // Resume happens on the close edge via the pause hook — every close
    // path shares it, including the toggle-hotkey close that used to
    // need its own resume here (the libretro core's EmuThread watchdog
    // kills a session left paused for 500 ms).
    m_inGameMenu->closeMenu();
}

bool AppController::inGameMenuOpen() const {
    return m_inGameMenu && m_inGameMenu->isMenuOpen();
}
