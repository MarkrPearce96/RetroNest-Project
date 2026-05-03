#pragma once

#include "core/manifest_loader.h"
#include "core/database.h"
#include "services/game_service.h"
#include "services/scraper_service.h"
#include "services/emulator_service.h"
#include "services/ra_service.h"
#include "services/config_service.h"
#include "adapters/emulator_adapter.h"
#include <QObject>
#include <QGuiApplication>
#include <QVariantList>
#include <QStringList>

class SdlInputManager;

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QStringList systems READ systems NOTIFY systemsChanged)
    Q_PROPERTY(QString currentSystem READ currentSystem WRITE setCurrentSystem NOTIFY currentSystemChanged)
    Q_PROPERTY(int currentTab READ currentTab WRITE setCurrentTab NOTIFY currentTabChanged)
    Q_PROPERTY(int settingsCategory READ settingsCategory WRITE setSettingsCategory NOTIFY settingsCategoryChanged)
    Q_PROPERTY(bool gameRunning READ isGameRunning NOTIFY gameRunningChanged)

public:
    AppController(ManifestLoader* loader, Database* db, QObject* parent = nullptr);
    void setSdlInputManager(SdlInputManager* mgr) { m_inputManager = mgr; }
    SdlInputManager* sdlInputManager() const { return m_inputManager; }

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
    Q_INVOKABLE void activateEmulator();

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
    Q_INVOKABLE QString browsePath(const QString& title);

    Q_INVOKABLE QString formatCapturedBinding(const QString& emuId, int deviceIndex,
                                               const QString& element, bool isAxis, bool positive) const;
    Q_INVOKABLE QString formatCapturedKeyboard(const QString& emuId, int qtKey, int modifiers) const;
    Q_INVOKABLE QString formatCapturedMouse(const QString& emuId, int qtButton) const;
    Q_INVOKABLE QString formatCapturedWheel(const QString& emuId, int direction) const;
    Q_INVOKABLE void showControllerMapping(const QString& emuId);
    Q_INVOKABLE void showEmulatorSettings(const QString& emuId);
    Q_INVOKABLE void openNativeEmulatorSettings(const QString& emuId);
    Q_INVOKABLE void showHotkeySettings(const QString& emuId);

    // Controller types (per-emulator)
    Q_INVOKABLE QVariantList controllerTypes(const QString& emuId) const;
    Q_INVOKABLE QString controllerType(const QString& emuId, int port) const;
    Q_INVOKABLE void setControllerType(const QString& emuId, int port, const QString& type);

    // Port-aware controller bindings
    Q_INVOKABLE QVariantList controllerBindingsForPort(const QString& emuId, int port) const;
    Q_INVOKABLE QVariantList controllerSettingsForPort(const QString& emuId, int port) const;
    Q_INVOKABLE void saveBindingForPort(const QString& emuId, int port, const QString& key, const QString& value);
    Q_INVOKABLE void clearBindingForPort(const QString& emuId, int port, const QString& key);
    Q_INVOKABLE void clearAllBindingsForPort(const QString& emuId, int port);
    Q_INVOKABLE void autoMapControllerForPort(const QString& emuId, int port, int deviceIndex);
    Q_INVOKABLE void saveControllerSettingForPort(const QString& emuId, int port,
                                                   const QString& key, const QString& value);
    Q_INVOKABLE void restoreDefaultsForPort(const QString& emuId, int port);

    // Profile management
    Q_INVOKABLE QStringList controllerProfiles(const QString& emuId) const;
    Q_INVOKABLE void createControllerProfile(const QString& emuId, const QString& name);
    Q_INVOKABLE void applyControllerProfile(const QString& emuId, const QString& name);
    Q_INVOKABLE void renameControllerProfile(const QString& emuId, const QString& oldName, const QString& newName);
    Q_INVOKABLE void deleteControllerProfile(const QString& emuId, const QString& name);

    // Cursor visibility (for settings overlay)
    Q_INVOKABLE void setCursorVisible(bool visible);

    // Hotkeys (per-emulator)
    Q_INVOKABLE QVariantList hotkeyBindings(const QString& emuId) const;
    Q_INVOKABLE void saveHotkey(const QString& emuId, const QString& section, const QString& key, const QString& value);
    Q_INVOKABLE void clearHotkey(const QString& emuId, const QString& section, const QString& key);
    Q_INVOKABLE void resetHotkeys(const QString& emuId);

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
    Q_INVOKABLE void raSetSoundEffects(bool enabled);

signals:
    void statusMessageChanged();
    void systemsChanged();
    void currentSystemChanged();
    void currentTabChanged();
    void settingsCategoryChanged();
    void gamesChanged();
    void gameRunningChanged();
    void gameStarted();
    void gameFinished(int exitCode, bool crashed);
    void globalHotkeyPressed();
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
    void raEmulatorLoginPrompt(const QString& emulatorName);
    void raUserSummaryReady(const QVariantMap& summary);
    void raUserGamesReady(const QVariantList& games);
    void raGameDetailReady(int raGameId, const QVariantMap& detail);
    void raGameIdLookupReady(const QString& title, int raGameId);

private:
    void setStatus(const QString& msg);

    ManifestLoader* m_loader;
    Database* m_db;
    SdlInputManager* m_inputManager = nullptr;
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
};
