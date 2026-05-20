#pragma once

#include "core/manifest_loader.h"
#include "core/database.h"
#include "core/game_session.h"
#include "services/game_service.h"
#include "services/scraper_service.h"
#include "services/emulator_service.h"
#include "services/ra_service.h"
#include "services/config_service.h"
#include <QObject>
#include <QGuiApplication>
#include <QPointer>
#include <QVariantList>
#include <QStringList>
#include <memory>

class SdlInputManager;
class InGameMenuController;
class PatchesInstaller;
class HotkeyMatcher;
class HotkeyDispatcher;
class QEvent;

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QStringList systems READ systems NOTIFY systemsChanged)
    Q_PROPERTY(QString currentSystem READ currentSystem WRITE setCurrentSystem NOTIFY currentSystemChanged)
    Q_PROPERTY(int currentTab READ currentTab WRITE setCurrentTab NOTIFY currentTabChanged)
    Q_PROPERTY(int settingsCategory READ settingsCategory WRITE setSettingsCategory NOTIFY settingsCategoryChanged)
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)
    // Single in-game menu visibility — routed by InGameMenuController to
    // whichever backend (floating NSPanel for external emulators, transparent
    // QQuickWindow for HW-render libretro) is presenting the menu.
    Q_PROPERTY(bool inGameMenuOpen READ inGameMenuOpen NOTIFY inGameMenuOpenChanged)
    // Set by QML overlays (e.g. SettingsOverlay) when they're visible. While true,
    // the libretro hotkey matcher ignores keyboard events so Esc / arrows / etc
    // reach the overlay's own focus routing instead of triggering ToggleMenu.
    Q_PROPERTY(bool libretroHotkeysSuppressed READ libretroHotkeysSuppressed WRITE setLibretroHotkeysSuppressed NOTIFY libretroHotkeysSuppressedChanged)
    Q_PROPERTY(GameSession* gameSession READ gameSession CONSTANT)

public:
    AppController(ManifestLoader* loader, Database* db, QObject* parent = nullptr);
    // Out-of-line destructor: required so std::unique_ptr<HotkeyMatcher /
    // HotkeyDispatcher> can be instantiated against the forward declarations
    // in this header — the deleter needs the full type, but only the .cpp
    // (where the headers are fully included) needs to see it.
    ~AppController() override;
    void setSdlInputManager(SdlInputManager* mgr);
    void attachPatchesInstaller(PatchesInstaller* installer);
    SdlInputManager* sdlInputManager() const { return m_inputManager; }

public slots:
    /** Triggered by the PCSX2 settings "Refresh PCSX2 patches" button (Task 7).
     *  Forces a fetch and surfaces both success AND failure toasts. */
    void refreshPcsx2Patches();

    // Game session (for QML binding to frameReady signal)
    GameSession* gameSession();

    // App state
    QString statusMessage() const;
    QStringList systems() const;
    QString currentSystem() const;
    void setCurrentSystem(const QString& sys);
    int currentTab() const;
    void setCurrentTab(int tab);
    int settingsCategory() const;
    void setSettingsCategory(int cat);

    // Game operations
    Q_INVOKABLE void importRoms();
    Q_INVOKABLE void scanRomFolders();
    void backfillSerials();
    Q_INVOKABLE QStringList importableSystems() const;
    Q_INVOKABLE void importRomsFromDir(const QString& dir, const QString& systemFilter);
    Q_INVOKABLE void launchGame(int gameId, const QString& romPath, const QString& emuId,
                                const QStringList& extraArgs = {});
    Q_INVOKABLE void removeGame(int gameId);
    Q_INVOKABLE void scrapeGame(int gameId);
    Q_INVOKABLE void scrapeGameWithProgress(int gameId);

    // Async game control
    bool isGameRunning() const;
    Q_INVOKABLE void stopGame();
    Q_INVOKABLE void saveAndStopGame(int slot);
    Q_INVOKABLE bool hasResumeState(const QString& romPath, const QString& emuId);
    Q_INVOKABLE QString resumeStateFile(const QString& romPath, const QString& emuId);
    Q_INVOKABLE void clearResumeState(const QString& romPath, const QString& emuId);

    // macOS Space switching (show our app over fullscreen emulator)
    Q_INVOKABLE void activateApp();

    // In-game menu — open/close routes to whichever backend matches the
    // currently-running game (external NSPanel vs HW-render libretro
    // overlay). Single API, picks backend internally.
    Q_INVOKABLE void openInGameMenu();
    Q_INVOKABLE void closeInGameMenu();
    bool inGameMenuOpen() const;
    bool libretroHotkeysSuppressed() const { return m_libretroHotkeysSuppressed; }
    Q_INVOKABLE void setLibretroHotkeysSuppressed(bool suppressed);

    // Inject the QML engine after construction so the panel can be
    // built lazily on first open. Called from main.cpp after the
    // engine is loaded.
    void setQmlEngine(class QQmlEngine* engine);

    // Emulator settings
    Q_INVOKABLE QVariantList allEmulatorStatus() const;
    Q_INVOKABLE void installEmulator(const QString& emuId);
    Q_INVOKABLE void uninstallEmulator(const QString& emuId);
    Q_INVOKABLE QVariantList biosStatus(const QString& emuId) const;
    Q_INVOKABLE void openBiosFolder();
    Q_INVOKABLE void openRomFolder();
    Q_INVOKABLE void checkForUpdates();

    // Config settings
    Q_INVOKABLE QVariantList settingsSchema(const QString& emuId) const;
    Q_INVOKABLE QString settingValue(const QString& emuId, const QString& section, const QString& key) const;
    Q_INVOKABLE void saveSettings(const QString& emuId, const QVariantMap& values);
    Q_INVOKABLE void resetConfiguration(const QString& emuId);

    // Settings session lifecycle — call from settings dialog ctor/dtor.
    // While a session is active, settingValue/saveSettings hit an in-memory
    // IniFile cache instead of opening + parsing the INI on every widget read.
    void beginSettingsSession(const QString& emuId);
    void endSettingsSession(const QString& emuId);

    // Quick settings (resolution / aspect ratio)
    Q_INVOKABLE QVariantList quickResolutionOptions(const QString& emuId) const;
    Q_INVOKABLE QString currentResolution(const QString& emuId) const;
    Q_INVOKABLE void applyQuickResolution(const QVariantMap& choices);
    Q_INVOKABLE QVariantList quickAspectRatioOptions(const QString& emuId) const;
    Q_INVOKABLE QString currentAspectRatio(const QString& emuId) const;
    Q_INVOKABLE void applyQuickAspectRatio(const QVariantMap& choices);

    // Path settings
    Q_INVOKABLE QVariantList pathDefs(const QString& emuId) const;
    Q_INVOKABLE QString pathValue(const QString& emuId, const QString& section, const QString& key) const;
    Q_INVOKABLE QString pathDefault(const QString& emuId, const QString& section, const QString& key) const;
    Q_INVOKABLE void savePaths(const QString& emuId, const QVariantMap& values);
    Q_INVOKABLE void resetPaths(const QString& emuId);
    Q_INVOKABLE QString browsePath(const QString& title);

    Q_INVOKABLE QString formatCapturedBinding(const QString& emuId, int deviceIndex,
                                               const QString& element, bool isAxis, bool positive) const;
    Q_INVOKABLE QString formatCapturedKeyboard(const QString& emuId, int qtKey, int modifiers) const;
    Q_INVOKABLE QString formatCapturedMouse(const QString& emuId, int qtButton) const;
    Q_INVOKABLE QString formatCapturedWheel(const QString& emuId, int direction) const;
    Q_INVOKABLE void showControllerMapping(const QString& emuId);
    Q_INVOKABLE void showControllerMapping(const QString& emuId,
                                            const QString& controllerTypeId);
    Q_INVOKABLE void showEmulatorSettings(const QString& emuId);
    Q_INVOKABLE void openNativeEmulatorSettings(const QString& emuId);
    Q_INVOKABLE void showHotkeySettings(const QString& emuId);
    Q_INVOKABLE void showLibretroHotkeySettings();

    // Controller types (per-emulator)
    Q_INVOKABLE QVariantList controllerTypes(const QString& emuId) const;
    Q_INVOKABLE QString controllerType(const QString& emuId, int port) const;
    // Port-aware controller bindings
    Q_INVOKABLE QVariantList controllerBindingsForPort(const QString& emuId, int port,
                                                        const QString& controllerTypeId) const;
    Q_INVOKABLE void saveBindingForPort(const QString& emuId, int port,
                                          const QString& controllerTypeId,
                                          const QString& key, const QString& value,
                                          int deviceIndex = -1);
    Q_INVOKABLE void clearBindingForPort(const QString& emuId, int port,
                                           const QString& controllerTypeId,
                                           const QString& key);
    Q_INVOKABLE void clearAllBindingsForPort(const QString& emuId, int port,
                                                const QString& controllerTypeId);
    Q_INVOKABLE void autoMapControllerForPort(const QString& emuId, int port,
                                                const QString& controllerTypeId,
                                                int deviceIndex);

    // Cursor visibility (for settings overlay)
    Q_INVOKABLE void setCursorVisible(bool visible);

    // Hotkeys (per-emulator)
    Q_INVOKABLE QVariantList hotkeyBindings(const QString& emuId) const;
    Q_INVOKABLE bool hasHotkeys(const QString& emuId) const;
    Q_INVOKABLE void saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value);
    Q_INVOKABLE void clearHotkey(const QString& emuId, const QString& section, const QString& key);
    Q_INVOKABLE void resetHotkeys(const QString& emuId);

    /** Reload all bindings from ConfigService into the libretro HotkeyMatcher
     *  using the libretro_hotkeys::kSentinelEmuId. Called from the AppController
     *  constructor and automatically from saveHotkey/clearHotkey/resetHotkeys
     *  when the emuId matches the sentinel, so live edits in the settings
     *  dialog take effect immediately without a relaunch. */
    Q_INVOKABLE void syncLibretroHotkeyBindings();

    // Scraper
    Q_INVOKABLE void validateScraperCredentials(const QString& user, const QString& pass);
    Q_INVOKABLE void scraperSignOut();
    Q_INVOKABLE bool hasScraperCredentials() const;
    Q_INVOKABLE QString scraperUsername() const;
    Q_INVOKABLE void startBatchScrape(const QStringList& mediaTypes,
                                       const QStringList& systems,
                                       const QString& gameFilter);
    Q_INVOKABLE void cancelScrape();
    Q_INVOKABLE QVariantList scrapableSystems() const;
    Q_INVOKABLE QStringList allMediaTypes() const;
    Q_INVOKABLE int scrapeGameCount(const QStringList& systems, const QString& gameFilter) const;

    // RetroAchievements
    Q_INVOKABLE void raLogin(const QString& username, const QString& apiKey);
    Q_INVOKABLE void raSignOut();
    Q_INVOKABLE bool hasRACredentials() const;
    Q_INVOKABLE QString raUsername() const;
    Q_INVOKABLE void raLoginWithPassword(const QString& username, const QString& password);
    Q_INVOKABLE bool raHasLibretroToken() const;
    // Async fetches — connect to ra*Ready signals for results.
    Q_INVOKABLE void raRequestUserSummary();
    Q_INVOKABLE void raRequestUserGames();
    Q_INVOKABLE void raRequestGameDetail(int raGameId);
    Q_INVOKABLE void raRequestGameIdLookup(const QString& title, const QString& system = {});
    Q_INVOKABLE QVariantMap currentGameInfo() const;
    Q_INVOKABLE void raProceedAfterLoginPrompt();
    Q_INVOKABLE bool raHardcoreMode() const;
    Q_INVOKABLE void raSetHardcoreMode(bool enabled);
    Q_INVOKABLE bool raNotifications() const;
    Q_INVOKABLE void raSetNotifications(bool enabled);
    Q_INVOKABLE bool raSoundEffects() const;

    /**
     * SP3.5: true iff the currently-running game is a libretro core whose
     * adapter advertises Pattern B HW rendering (PCSX2 today; DuckStation /
     * PPSSPP / Dolphin when those land as libretro). Used by the floating
     * LibretroOverlayPanel + AppWindow.qml's toggleInGameMenu branch to
     * route overlays through the path that renders above the game's Metal
     * NSView.
     */
    Q_INVOKABLE bool gameUsesHardwareRender();
    Q_INVOKABLE void raSetSoundEffects(bool enabled);
    Q_INVOKABLE bool raEncoreMode() const;
    /** Toggling encore mid-session takes effect on the running rc_client
     *  immediately — no relaunch needed. The setting also persists in
     *  retroachievements.json so the next session picks it up. */
    Q_INVOKABLE void raSetEncoreMode(bool enabled);
    /** True iff a libretro game is currently running AND its rcheevos
     *  session has finished loading. QML uses this to pick between the
     *  in-memory rc_client achievement list and the RA web API.
     *  Non-const because GameService::session() is non-const. */
    Q_INVOKABLE bool libretroAchievementsReady();
    /** Snapshot of the current libretro game's CORE achievement list
     *  drawn straight from rc_client (no network). Each entry is a
     *  QVariantMap with: id, title, description, points, earned,
     *  badgeUrl, measured. Returns empty if no libretro session is
     *  active or rcheevos is still loading. */
    Q_INVOKABLE QVariantList libretroAchievementList();

signals:
    void statusMessageChanged();
    void systemsChanged();
    void currentSystemChanged();
    void currentTabChanged();
    void settingsCategoryChanged();
    void gamesChanged();
    void gameRunningChanged();
    void inGameMenuOpenChanged();
    void libretroHotkeysSuppressedChanged();
    void gameStarted();
    /** Emitted instead of gameStarted() when the backend is libretro (in-process). */
    void gameStartedLibretro();
    /** Emitted BEFORE a libretro game starts (before retro_load_game runs).
     *  AppWindow uses this to pre-push EmulationView so LibretroMetalItem's
     *  NSView is realised and registered with CoreRuntime before the core's
     *  Host::AcquireRenderWindow queries it. SP3 launch-ordering fix. */
    void gameStartingLibretro();
    void gameFinished(int exitCode, bool crashed);
    void globalHotkeyPressed();
    // Fired by the libretro hotkey matcher when the user's ToggleMenu
    // binding is pressed. Distinguished from globalHotkeyPressed so QML
    // can keep the macOS Cmd+Shift+Esc hotkey scoped to standalone
    // emulators while libretro games drive their menu via this signal.
    void libretroMenuToggleRequested();
    void emulatorInstalled(const QString& emuId);
    void installProgress(const QString& emuId, double progress,
                         const QString& phase, const QString& detail);
    void installFinished(const QString& emuId, bool success, const QString& message);
    void uninstallFinished(const QString& emuId, bool success, const QString& message);
    void configurationReset(const QString& emuId);
    void scraperCredentialsValidated(bool success, const QString& message);
    void scraperSignedOut();
    void scrapeProgress(int current, int total, const QVariantMap& gameData);
    void scrapeFinished(int succeeded, int failed, int skipped);
    void updateAvailable(const QString& emuId, const QString& currentVersion,
                         const QString& latestVersion);
    void raLoginCompleted(bool success, const QString& message);
    void raSignedOut();
    void raLoginTokenChanged();
    void raLoginFailed(const QString& message);
    void raEmulatorLoginPrompt(const QString& emulatorName);
    void raUserSummaryReady(const QVariantMap& summary);
    void raUserGamesReady(const QVariantList& games);
    void raGameDetailReady(int raGameId, const QVariantMap& detail);
    void raGameIdLookupReady(const QString& title, int raGameId);
    /** Forwarded from RAService when an in-process libretro achievement
     *  triggers. QML uses this to render the unlock toast. `imageUrl`
     *  is the unlocked-state badge URL provided by rc_client (empty if
     *  the runtime couldn't resolve it). */
    void raAchievementUnlocked(const QString& id, const QString& title,
                               const QString& description,
                               const QString& imageUrl);
    /** Generic info toast forwarded from RAService — used for the
     *  game-start banner, game-mastered celebration, hardcore reset
     *  notice, and server-error notice. QML routes this through the
     *  same AchievementToast component as the unlock toast. */
    void raInfoToast(const QString& header, const QString& title,
                     const QString& description, const QString& imageUrl,
                     int durationMs);
    /** Indicator-bar update forwarded from RAService — challenge /
     *  progress chips and connection-status banner. `kind` matches
     *  rc_client event-type integers; `data` carries the per-event
     *  payload. QML drives RAIndicatorBar from this. */
    void raIndicator(int kind, const QVariantMap& data);

protected:
    /** App-wide Qt key-event tap. Forwards QKeyEvent press/release edges
     *  into the libretro HotkeyMatcher. Never consumes events (returns
     *  false) so normal Qt focus routing continues unchanged — the matcher
     *  itself drops events for unbound actions. AutoRepeat events are
     *  filtered out so a held key emits a single actionPressed. */
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setStatus(const QString& msg);
    bool m_patchesManualRefresh = false;  // true while a user-triggered refresh is in flight

    /** Emit the PCSX2 Patches toast with success/failure formatting. */
    void emitPatchesToast(bool success, const QString& message);

    ManifestLoader* m_loader;
    Database* m_db;
    SdlInputManager* m_inputManager = nullptr;
    QPointer<PatchesInstaller> m_patchesInstaller;
    GameService m_gameService;
    ScraperService m_scraperService;
    EmulatorService m_emuService;
    RAService m_raService;
    ConfigService m_configService;

    // Pending launch (deferred while RA login prompt is shown)
    QString m_pendingLaunchRom;
    QString m_pendingLaunchEmu;
    QStringList m_pendingLaunchArgs;

    QString m_statusMessage;
    QString m_currentSystem;
    int m_currentTab = 0;
    int m_settingsCategory = 0;

    // setCursorVisible() state. false at startup to match the initial
    // BlankCursor push in main.cpp, so the first setCursorVisible(true)
    // actually pops. Tracked explicitly to keep the function idempotent
    // against repeated calls — QGuiApplication's override cursor is a
    // stack, and naive paired push/pop is imbalanced by rapid
    // open/close cycles during the settings overlay slide-out animation.
    bool m_cursorVisible = false;
    bool m_libretroHotkeysSuppressed = false;

    QQmlEngine* m_qmlEngine = nullptr;

    // Single owner for both in-game menu surfaces (external NSPanel +
    // HW-render libretro overlay). Lazy-created on setQmlEngine(); routes
    // openInGameMenu() to whichever backend matches the running game.
    InGameMenuController* m_inGameMenu = nullptr;

    // True between paired Space-keystroke sends to the emulator
    // (the TogglePause hotkey is a toggle, so we must track which
    // half of the toggle we're in to avoid double-pause/double-resume).
    bool m_emulatorSuspended = false;

    // Polls SDL state at 16ms intervals after closeInGameMenu()
    // until all action buttons (A/B/X/Y) are released, then sends
    // the Space keystroke to unpause. Variable delay so the close-
    // trigger button can never leak as in-game input.
    QTimer* m_resumeWhenButtonsReleasedTimer = nullptr;

    // App-global libretro hotkey routing. AppController owns the matcher
    // (not CoreRuntime) because libretro hotkeys are app-global — the same
    // bindings apply across game launches and the matcher must persist
    // longer than any single CoreRuntime instance. The dispatcher routes
    // action signals through m_gameService.session() to the currently-
    // active GameSession / LibretroAdapter / CoreRuntime / AudioSink.
    std::unique_ptr<HotkeyMatcher>    m_hotkeyMatcher;
    std::unique_ptr<HotkeyDispatcher> m_hotkeyDispatcher;
};
