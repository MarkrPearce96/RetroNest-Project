#include "app_controller.h"
#include "adapters/adapter_registry.h"
#include "core/ini_file.h"
#include "core/macos_fullscreen.h"
#include "core/paths.h"
#include "core/scraper.h"
#include "core/setting_def.h"

#include <QCursor>
#include "settings/controller_mapping_page.h"
#include "settings/emulator_settings_page.h"
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
{
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

QVariantList AppController::settingsSchema(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    for (const auto& def : adapter->settingsSchema()) {
        QVariantMap item;
        item["label"] = def.label;
        item["category"] = def.category;
        item["subcategory"] = def.subcategory;
        item["group"] = def.group;
        item["section"] = def.section;
        item["key"] = def.key;
        item["type"] = static_cast<int>(def.type);
        item["defaultValue"] = def.defaultValue;
        item["minVal"] = def.minVal;
        item["maxVal"] = def.maxVal;
        item["step"] = def.step;
        QVariantList opts;
        QVariantMap optValues;
        for (const auto& pair : def.options) {
            opts.append(pair.first);  // display label for ComboBox
            optValues[pair.first] = pair.second;  // label → INI value
        }
        item["options"] = opts;
        item["optionValues"] = optValues;
        item["tooltip"] = def.tooltip;
        item["layout"] = def.layout;
        item["suffix"] = def.suffix;
        list.append(item);
    }
    return list;
}

QString AppController::settingValue(const QString& emuId, const QString& section, const QString& key) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return {};

    IniFile ini;
    ini.load(configPath);
    return ini.value(section, key);
}

void AppController::saveSettings(const QString& emuId, const QVariantMap& values) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);

    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        // Key format: "section/key" — split on LAST '/' since sections can contain '/'
        // e.g. "EmuCore/GS/Renderer" → section="EmuCore/GS", key="Renderer"
        int lastSlash = it.key().lastIndexOf('/');
        if (lastSlash > 0) {
            QString section = it.key().left(lastSlash);
            QString key = it.key().mid(lastSlash + 1);
            ini.setValue(section, key, it.value().toString());
        } else {
            qWarning() << "[Settings] Skipping malformed key (no section separator):" << it.key();
        }
    }

    if (ini.save(configPath))
        setStatus("Settings saved.");
    else
        setStatus("Failed to save settings.");
}

void AppController::resetConfiguration(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const auto* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return;

    // Delete the existing config file
    QString configPath = adapter->configFilePath();
    if (!configPath.isEmpty() && QFileInfo::exists(configPath)) {
        QFile::remove(configPath);
        qInfo() << "[Reset] Removed config:" << configPath;
    }

    // Re-run ensureConfig to regenerate the fresh install defaults
    QString systemId = Paths::systemIdFor(emuId, manifest->systems);
    QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    QString savesPath = QFileInfo(Paths::savesDir(systemId)).absoluteFilePath();
    adapter->ensureConfig(*manifest, biosPath, savesPath);

    // Also reset controller bindings and settings to defaults
    resetControllerBindings(emuId);
    resetControllerSettings(emuId);

    // Reset hotkeys to defaults for this emulator
    resetHotkeys(emuId);

    setStatus(manifest->name + " configuration reset to install defaults.");
    emit configurationReset(emuId);
}

// ── Quick Settings (Resolution / Aspect Ratio) ───────────

QVariantList AppController::quickResolutionOptions(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    auto opts = adapter->resolutionOptions();
    for (const auto& opt : opts.options) {
        QVariantMap item;
        item["label"] = opt.label;
        item["value"] = opt.value;
        list.append(item);
    }
    return list;
}

QString AppController::currentResolution(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->resolutionOptions();
    if (opts.options.isEmpty()) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return opts.defaultValue;

    IniFile ini;
    ini.load(configPath);
    QString val = ini.value(opts.section, opts.key);
    return val.isEmpty() ? opts.defaultValue : val;
}

void AppController::applyQuickResolution(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString value = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->resolutionOptions();
        if (opts.options.isEmpty()) continue;

        QString configPath = adapter->configFilePath();
        if (configPath.isEmpty()) continue;

        IniFile ini;
        ini.load(configPath);
        ini.setValue(opts.section, opts.key, value);
        ini.save(configPath);
    }
    setStatus("Resolution settings saved.");
}

QVariantList AppController::quickAspectRatioOptions(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    auto opts = adapter->aspectRatioOptions();
    for (const auto& opt : opts.options) {
        QVariantMap item;
        item["label"] = opt.label;
        list.append(item);
    }
    return list;
}

QString AppController::currentAspectRatio(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->aspectRatioOptions();
    if (opts.options.isEmpty()) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return opts.defaultLabel;

    IniFile ini;
    ini.load(configPath);

    // Check which option matches the current INI state by comparing the first patch key
    for (const auto& opt : opts.options) {
        if (opt.patches.isEmpty()) continue;
        const auto& firstPatch = opt.patches.first();
        QString val = ini.value(firstPatch.section, firstPatch.key);
        if (val == firstPatch.value)
            return opt.label;
    }
    return opts.defaultLabel;
}

void AppController::applyQuickAspectRatio(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString label = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->aspectRatioOptions();

        // Find the matching option by label
        for (const auto& opt : opts.options) {
            if (opt.label != label) continue;

            QString configPath = adapter->configFilePath();
            if (configPath.isEmpty()) break;

            IniFile ini;
            ini.load(configPath);

            // Write ALL patches for this option (e.g. aspect ratio + widescreen patches)
            for (const auto& patch : opt.patches)
                ini.setValue(patch.section, patch.key, patch.value);

            ini.save(configPath);
            break;
        }
    }
    setStatus("Aspect ratio settings saved.");
}

// ── Path Settings ──────────────────────────────────────────

QVariantList AppController::pathDefs(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    const auto* manifest = m_loader->emulatorById(emuId);
    QString systemId = manifest ? Paths::systemIdFor(emuId, manifest->systems) : emuId;

    for (const auto& pd : adapter->pathsDefs()) {
        QVariantMap item;
        item["label"] = pd.label;
        item["section"] = pd.section;
        item["key"] = pd.key;
        // Compute default path
        QString defPath;
        switch (pd.base) {
            case PathBase::Bios:
                defPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
                break;
            case PathBase::Saves:
                defPath = QFileInfo(Paths::savesDir(systemId) + "/" + pd.defaultSuffix).absoluteFilePath();
                break;
            case PathBase::Data:
                defPath = QFileInfo(Paths::dataDir(emuId) + "/" + pd.defaultSuffix).absoluteFilePath();
                break;
        }
        item["defaultPath"] = defPath;
        list.append(item);
    }
    return list;
}

QString AppController::pathValue(const QString& emuId, const QString& section, const QString& key) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return {};

    IniFile ini;
    ini.load(configPath);
    return ini.value(section, key);
}

QString AppController::pathDefault(const QString& emuId, const QString& section, const QString& key) const {
    auto defs = pathDefs(emuId);
    for (const auto& d : defs) {
        auto map = d.toMap();
        if (map["section"].toString() == section && map["key"].toString() == key)
            return map["defaultPath"].toString();
    }
    return {};
}

void AppController::savePaths(const QString& emuId, const QVariantMap& values) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);

    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        int lastSlash = it.key().lastIndexOf('/');
        if (lastSlash > 0) {
            QString section = it.key().left(lastSlash);
            QString key = it.key().mid(lastSlash + 1);
            ini.setValue(section, key, it.value().toString());
        } else {
            qWarning() << "[Paths] Skipping malformed key (no section separator):" << it.key();
        }
    }

    if (ini.save(configPath))
        setStatus("Paths saved.");
    else
        setStatus("Failed to save paths.");
}

QString AppController::browsePath(const QString& title) {
    return QFileDialog::getExistingDirectory(nullptr, title,
        QDir::homePath(), QFileDialog::ShowDirsOnly);
}

// ── Controller Settings (Settings sub-tab) ────────────────

QVariantList AppController::controllerSettings(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QVariantList list;
    for (const auto& def : adapter->controllerSettingDefs()) {
        QVariantMap item;
        item["label"] = def.label;
        item["tooltip"] = def.tooltip;
        item["section"] = def.section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;
        item["type"] = static_cast<int>(def.type);
        item["suffix"] = def.suffix;
        item["minVal"] = def.minVal;
        item["maxVal"] = def.maxVal;
        item["step"] = def.step;

        QVariantList opts;
        for (const auto& opt : def.options) {
            QVariantMap o;
            o["label"] = opt.first;
            o["value"] = opt.second;
            opts.append(o);
        }
        item["options"] = opts;

        QString val = ini.value(def.section, def.key);
        item["currentValue"] = val.isEmpty() ? def.defaultValue : val;
        list.append(item);
    }
    return list;
}

void AppController::saveControllerSetting(const QString& emuId, const QString& section,
                                           const QString& key, const QString& value) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    ini.setValue(section, key, value);

    if (ini.save(configPath))
        setStatus("Controller setting saved.");
    else
        setStatus("Failed to save controller setting.");
}

void AppController::resetControllerSettings(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerSettingDefs())
        ini.setValue(def.section, def.key, def.defaultValue);

    if (ini.save(configPath))
        setStatus("Controller settings reset to defaults.");
    else
        setStatus("Failed to reset controller settings.");
}

// ── Controller Bindings ───────────────────────────────────

QVariantList AppController::controllerBindings(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QVariantList list;
    for (const auto& def : adapter->controllerBindingDefs()) {
        QVariantMap item;
        item["label"] = def.label;
        item["group"] = def.group;
        item["section"] = def.section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;
        item["kind"] = static_cast<int>(def.kind);

        item["currentValue"] = ini.value(def.section, def.key);
        list.append(item);
    }
    return list;
}

void AppController::saveBinding(const QString& emuId, const QString& section,
                                 const QString& key, const QString& value) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    ini.setValue(section, key, value);

    if (ini.save(configPath))
        setStatus("Binding saved.");
    else
        setStatus("Failed to save binding.");
}

void AppController::clearBinding(const QString& emuId, const QString& section, const QString& key) {
    saveBinding(emuId, section, key, "");
}

void AppController::resetControllerBindings(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefs())
        ini.setValue(def.section, def.key, def.defaultValue);

    if (ini.save(configPath))
        setStatus("Controller bindings reset to defaults.");
    else
        setStatus("Failed to reset controller bindings.");
}

void AppController::clearControllerBindings(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefs())
        ini.setValue(def.section, def.key, "");

    if (ini.save(configPath))
        setStatus("Controller bindings cleared.");
    else
        setStatus("Failed to clear controller bindings.");
}

void AppController::autoMapController(const QString& emuId, int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);

    // Write default bindings, replacing SDL-0 with the selected device index
    QString fromPrefix = QStringLiteral("SDL-0/");
    QString toPrefix = QString("SDL-%1/").arg(deviceIndex);

    for (const auto& def : adapter->controllerBindingDefs()) {
        QString value = def.defaultValue;
        if (!value.isEmpty())
            value.replace(fromPrefix, toPrefix);
        ini.setValue(def.section, def.key, value);
    }

    if (ini.save(configPath))
        setStatus(QString("Controller mapped to SDL-%1.").arg(deviceIndex));
    else
        setStatus("Failed to save controller mappings.");
}

QString AppController::formatCapturedBinding(const QString& emuId, int deviceIndex,
                                              const QString& element, bool isAxis, bool positive) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatBinding(deviceIndex, element, isAxis, positive);
}

QString AppController::formatCapturedKeyboard(const QString& emuId, int qtKey, int modifiers) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatKeyboardBinding(qtKey, modifiers);
}

QString AppController::formatCapturedMouse(const QString& emuId, int qtButton) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatMouseBinding(qtButton);
}

QString AppController::formatCapturedWheel(const QString& emuId, int direction) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatWheelBinding(direction);
}

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
    auto* dialog = new EmulatorSettingsPage(this, emuId);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
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
    adapter->ensureConfig(*manifest, Paths::biosDir(),
                          Paths::savesDir(manifest->systems.value(0)));

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
    if (visible) {
        QGuiApplication::restoreOverrideCursor();
    } else {
        QGuiApplication::setOverrideCursor(Qt::BlankCursor);
    }
}

// ── Hotkeys (per-emulator) ────────────────────────────────

QVariantList AppController::hotkeyBindings(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto defs = adapter->hotkeyBindingDefs();
    if (defs.isEmpty()) return {};

    QString configPath = adapter->controllerBindingsConfigFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QVariantList list;
    for (const auto& def : defs) {
        QVariantMap item;
        item["label"] = def.label;
        item["group"] = def.group;
        item["section"] = def.section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;

        QString current = ini.value(def.section, def.key);
        item["currentValue"] = current.isEmpty() ? def.defaultValue : current;
        list.append(item);
    }
    return list;
}

void AppController::saveHotkey(const QString& emuId, const QString& section,
                                const QString& key, const QString& value) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    ini.setValue(section, key, value);

    if (!ini.save(configPath))
        setStatus("Failed to save hotkey.");
}

void AppController::clearHotkey(const QString& emuId, const QString& section, const QString& key) {
    saveHotkey(emuId, section, key, "");
}

void AppController::resetHotkeys(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    for (const auto& def : adapter->hotkeyBindingDefs())
        ini.setValue(def.section, def.key, def.defaultValue);

    if (ini.save(configPath))
        setStatus("Hotkeys reset to defaults.");
    else
        setStatus("Failed to reset hotkeys.");
}

// ── Controller Types ────────────────────────────────────────

QVariantList AppController::controllerTypes(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QVariantList list;
    for (const auto& t : adapter->controllerTypes()) {
        QVariantMap item;
        item["id"] = t.id;
        item["displayName"] = t.displayName;
        item["svgResource"] = t.svgResource;
        list.append(item);
    }
    return list;
}

QString AppController::controllerType(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return "NotConnected";

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return "NotConnected";

    IniFile ini;
    ini.load(configPath);
    QString section = QString("Pad%1").arg(port);
    QString type = ini.value(section, "Type");
    return type.isEmpty() ? "DualShock2" : type;
}

void AppController::setControllerType(const QString& emuId, int port, const QString& type) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = QString("Pad%1").arg(port);
    ini.setValue(section, "Type", type);

    if (ini.save(configPath))
        setStatus(QString("Controller type set to %1.").arg(type));
    else
        setStatus("Failed to save controller type.");
}

// ── Port-Aware Controller Bindings ──────────────────────────

QVariantList AppController::controllerBindingsForPort(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    // Controller type is stored in the main config file under Pad{port}/Type
    QString mainConfigPath = adapter->configFilePath();
    IniFile mainIni;
    if (!mainConfigPath.isEmpty())
        mainIni.load(mainConfigPath);

    QString type = mainIni.value(QString("Pad%1").arg(port), "Type");
    if (type.isEmpty()) type = "DualShock2";

    // Bindings may live in a different file/section (e.g., PPSSPP's controls.ini)
    QString bindingsPath = adapter->controllerBindingsConfigFilePath();
    IniFile bindingsIni;
    if (!bindingsPath.isEmpty())
        bindingsIni.load(bindingsPath);

    QString section = adapter->controllerBindingsSection(port);

    QVariantList list;
    for (const auto& def : adapter->controllerBindingDefsForType(type)) {
        QVariantMap item;
        item["label"] = def.label;
        item["group"] = def.group;
        item["section"] = section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;
        item["currentValue"] = bindingsIni.value(section, def.key);
        list.append(item);
    }
    return list;
}

QVariantList AppController::controllerSettingsForPort(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QString type = ini.value(QString("Pad%1").arg(port), "Type");
    if (type.isEmpty()) type = "DualShock2";

    QString section = QString("Pad%1").arg(port);

    QVariantList list;
    for (const auto& def : adapter->controllerSettingDefsForType(type)) {
        QVariantMap item;
        item["label"] = def.label;
        item["tooltip"] = def.tooltip;
        item["section"] = section;
        item["key"] = def.key;
        item["defaultValue"] = def.defaultValue;
        item["type"] = static_cast<int>(def.type);
        item["suffix"] = def.suffix;
        item["minVal"] = def.minVal;
        item["maxVal"] = def.maxVal;
        item["step"] = def.step;

        QVariantList opts;
        for (const auto& opt : def.options) {
            QVariantMap o;
            o["label"] = opt.first;
            o["value"] = opt.second;
            opts.append(o);
        }
        item["options"] = opts;

        QString val = ini.value(section, def.key);
        item["currentValue"] = val.isEmpty() ? def.defaultValue : val;
        list.append(item);
    }
    return list;
}

void AppController::saveBindingForPort(const QString& emuId, int port,
                                        const QString& key, const QString& value) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = adapter->controllerBindingsSection(port);
    ini.setValue(section, key, value);

    if (!ini.save(configPath))
        setStatus("Failed to save binding.");
}

void AppController::clearBindingForPort(const QString& emuId, int port, const QString& key) {
    saveBindingForPort(emuId, port, key, "");
}

void AppController::clearAllBindingsForPort(const QString& emuId, int port) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    QString type = controllerType(emuId, port);
    QString section = adapter->controllerBindingsSection(port);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(type))
        ini.setValue(section, def.key, "");

    if (ini.save(configPath))
        setStatus("Bindings cleared.");
    else
        setStatus("Failed to clear bindings.");
}

void AppController::autoMapControllerForPort(const QString& emuId, int port, int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    QString type = controllerType(emuId, port);
    QString section = adapter->controllerBindingsSection(port);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(type)) {
        QString mapped = def.defaultValue;
        if (!mapped.isEmpty() && deviceIndex != 0) {
            mapped.replace("SDL-0/", QString("SDL-%1/").arg(deviceIndex));
        }
        ini.setValue(section, def.key, mapped);
    }

    if (ini.save(configPath))
        setStatus("Controller auto-mapped.");
    else
        setStatus("Failed to auto-map controller.");
}

void AppController::saveControllerSettingForPort(const QString& emuId, int port,
                                                  const QString& key, const QString& value) {
    // Controller settings (deadzone, sensitivity) live in the main config file
    // under Pad{port}, even for emulators that store bindings elsewhere.
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = QString("Pad%1").arg(port);
    ini.setValue(section, key, value);

    if (!ini.save(configPath))
        setStatus("Failed to save controller setting.");
}

void AppController::restoreDefaultsForPort(const QString& emuId, int port) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString type = controllerType(emuId, port);

    // Restore bindings to defaults (may live in a separate file/section)
    QString bindingsPath = adapter->controllerBindingsConfigFilePath();
    if (!bindingsPath.isEmpty()) {
        IniFile bindingsIni;
        bindingsIni.load(bindingsPath);
        QString bindingsSection = adapter->controllerBindingsSection(port);
        for (const auto& def : adapter->controllerBindingDefsForType(type))
            bindingsIni.setValue(bindingsSection, def.key, def.defaultValue);
        bindingsIni.save(bindingsPath);
    }

    // Reset controller settings to defaults (always in main configFilePath + Pad{port})
    QString configPath = adapter->configFilePath();
    if (!configPath.isEmpty()) {
        IniFile ini;
        ini.load(configPath);
        QString section = QString("Pad%1").arg(port);
        for (const auto& def : adapter->controllerSettingDefsForType(type))
            ini.setValue(section, def.key, def.defaultValue);
        ini.save(configPath);
    }

    setStatus("Controller defaults restored.");
}

// ── Controller Profile Management ───────────────────────────

QStringList AppController::controllerProfiles(const QString& emuId) const {
    Q_UNUSED(emuId);
    QString profileDir = Paths::configDir() + "/controller_profiles";
    QDir dir(profileDir);
    QStringList profiles;
    if (dir.exists()) {
        for (const auto& entry : dir.entryList({"*.ini"}, QDir::Files))
            profiles.append(entry.chopped(4)); // remove .ini
    }
    return profiles;
}

static QString sanitizeProfileName(const QString& name) {
    QString safe = name;
    safe.remove(QRegularExpression("[/\\\\:*?\"<>|.]"));
    return safe.trimmed();
}

void AppController::createControllerProfile(const QString& emuId, const QString& name) {
    QString safeName = sanitizeProfileName(name);
    if (safeName.isEmpty()) return;

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString profileDir = Paths::configDir() + "/controller_profiles";
    QDir().mkpath(profileDir);

    QString srcPath = adapter->configFilePath();
    QString dstPath = profileDir + "/" + safeName + ".ini";

    if (srcPath.isEmpty()) return;

    // Copy current pad sections to profile
    IniFile src;
    src.load(srcPath);

    IniFile dst;
    for (int port = 1; port <= 2; port++) {
        QString section = QString("Pad%1").arg(port);
        for (const auto& key : src.keys(section))
            dst.setValue(section, key, src.value(section, key));
    }

    if (dst.save(dstPath))
        setStatus(QString("Profile '%1' created.").arg(safeName));
    else
        setStatus("Failed to create profile.");
}

void AppController::applyControllerProfile(const QString& emuId, const QString& name) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString profileDir = Paths::configDir() + "/controller_profiles";
    QString profilePath = profileDir + "/" + name + ".ini";
    QString configPath = adapter->configFilePath();

    if (configPath.isEmpty() || !QFileInfo::exists(profilePath)) return;

    IniFile profile;
    profile.load(profilePath);

    IniFile config;
    config.load(configPath);

    for (int port = 1; port <= 2; port++) {
        QString section = QString("Pad%1").arg(port);
        for (const auto& key : profile.keys(section))
            config.setValue(section, key, profile.value(section, key));
    }

    if (config.save(configPath))
        setStatus(QString("Profile '%1' applied.").arg(name));
    else
        setStatus("Failed to apply profile.");
}

void AppController::renameControllerProfile(const QString& emuId, const QString& oldName,
                                             const QString& newName) {
    Q_UNUSED(emuId);
    QString safeNewName = sanitizeProfileName(newName);
    if (safeNewName.isEmpty()) return;

    QString profileDir = Paths::configDir() + "/controller_profiles";
    QString oldPath = profileDir + "/" + oldName + ".ini";
    QString newPath = profileDir + "/" + safeNewName + ".ini";

    if (QFile::rename(oldPath, newPath))
        setStatus(QString("Profile renamed to '%1'.").arg(safeNewName));
    else
        setStatus("Failed to rename profile.");
}

void AppController::deleteControllerProfile(const QString& emuId, const QString& name) {
    Q_UNUSED(emuId);
    QString safeName = sanitizeProfileName(name);
    if (safeName.isEmpty()) return;

    QString profileDir = Paths::configDir() + "/controller_profiles";
    QString path = profileDir + "/" + safeName + ".ini";

    if (QFile::remove(path))
        setStatus(QString("Profile '%1' deleted.").arg(safeName));
    else
        setStatus("Failed to delete profile.");
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
QVariantMap AppController::raUserSummary() { return m_raService.userSummary(); }
QVariantList AppController::raUserGames() { return m_raService.userGames(); }
QVariantMap AppController::raGameDetail(int raGameId) { return m_raService.gameDetail(raGameId); }
int AppController::raFindGameId(const QString& title, const QString& system) { return m_raService.findRaGameId(title, system); }

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
