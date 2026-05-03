#include "config_service.h"

#include "adapters/adapter_registry.h"
#include "core/ini_file.h"
#include "core/paths.h"
#include "core/setting_def.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

// Returns iniFilePath if non-empty, else adapter->configFilePath().
// Used by quick-settings paths so adapters can target a non-main INI file
// (e.g. Dolphin's GFX.ini for resolution/aspect).
static QString resolveConfigPath(const QString& iniFilePath, EmulatorAdapter* adapter) {
    return iniFilePath.isEmpty() ? adapter->configFilePath() : iniFilePath;
}

ConfigService::ConfigService(ManifestLoader* loader, QObject* parent)
    : QObject(parent), m_loader(loader) {}

ConfigService::~ConfigService() = default;

// ── Settings ──────────────────────────────────────────────

QVariantList ConfigService::settingsSchema(const QString& emuId) const {
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
            opts.append(pair.first);
            optValues[pair.first] = pair.second;
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

QString ConfigService::settingValue(const QString& emuId, const QString& section, const QString& key) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return {};

    if (m_settingsCache && m_settingsCachePath == configPath)
        return m_settingsCache->value(section, key);

    IniFile ini;
    ini.load(configPath);
    return ini.value(section, key);
}

void ConfigService::saveSettings(const QString& emuId, const QVariantMap& values) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    const bool useCache = m_settingsCache && m_settingsCachePath == configPath;
    IniFile localIni;
    IniFile& ini = useCache ? *m_settingsCache : localIni;
    if (!useCache)
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
        emit statusMessage("Settings saved.");
    else
        emit statusMessage("Failed to save settings.");
}

void ConfigService::beginSettingsSession(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;
    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    m_settingsCache = std::make_unique<IniFile>();
    m_settingsCache->load(configPath);
    m_settingsCachePath = configPath;
}

void ConfigService::endSettingsSession(const QString& /*emuId*/) {
    m_settingsCache.reset();
    m_settingsCachePath.clear();
}

void ConfigService::resetConfiguration(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const auto* manifest = m_loader->emulatorById(emuId);
    if (!manifest) return;

    QString configPath = adapter->configFilePath();
    if (!configPath.isEmpty() && QFileInfo::exists(configPath)) {
        QFile::remove(configPath);
        qInfo() << "[Reset] Removed config:" << configPath;
    }

    if (m_settingsCachePath == configPath) {
        m_settingsCache.reset();
        m_settingsCachePath.clear();
    }

    QString systemId = Paths::systemIdFor(emuId, manifest->systems);
    QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
    QString dataPath = QFileInfo(Paths::emulatorDataDir(emuId, systemId)).absoluteFilePath();
    adapter->ensureConfig(*manifest, biosPath, dataPath);

    for (int port = 1; port <= 2; ++port)
        restoreDefaultsForPort(emuId, port);

    resetHotkeys(emuId);

    emit statusMessage(manifest->name + " configuration reset to install defaults.");
    emit configurationReset(emuId);
}

// ── Quick Settings ──────────────────────────────────────────

QVariantList ConfigService::quickResolutionOptions(const QString& emuId) const {
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

QString ConfigService::currentResolution(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->resolutionOptions();
    if (opts.options.isEmpty()) return {};

    QString configPath = resolveConfigPath(opts.iniFilePath, adapter);
    if (configPath.isEmpty()) return opts.defaultValue;

    IniFile ini;
    ini.load(configPath);
    QString val = ini.value(opts.section, opts.key);
    return val.isEmpty() ? opts.defaultValue : val;
}

void ConfigService::applyQuickResolution(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString value = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->resolutionOptions();
        if (opts.options.isEmpty()) continue;

        QString configPath = resolveConfigPath(opts.iniFilePath, adapter);
        if (configPath.isEmpty()) continue;

        IniFile ini;
        ini.load(configPath);
        ini.setValue(opts.section, opts.key, value);
        ini.save(configPath);
    }
    emit statusMessage("Resolution settings saved.");
}

QVariantList ConfigService::quickAspectRatioOptions(const QString& emuId) const {
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

QString ConfigService::currentAspectRatio(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    auto opts = adapter->aspectRatioOptions();
    if (opts.options.isEmpty()) return {};

    // Match by comparing the first patch — whichever option's first patch
    // value matches what's on disk (in its own file) is the current selection.
    for (const auto& opt : opts.options) {
        if (opt.patches.isEmpty()) continue;
        const auto& firstPatch = opt.patches.first();
        const QString path = resolveConfigPath(firstPatch.iniFilePath, adapter);
        if (path.isEmpty()) continue;

        IniFile ini;
        ini.load(path);
        QString val = ini.value(firstPatch.section, firstPatch.key);
        if (val == firstPatch.value)
            return opt.label;
    }
    return opts.defaultLabel;
}

void ConfigService::applyQuickAspectRatio(const QVariantMap& choices) {
    for (auto it = choices.constBegin(); it != choices.constEnd(); ++it) {
        const QString& emuId = it.key();
        const QString label = it.value().toString();

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        auto opts = adapter->aspectRatioOptions();

        for (const auto& opt : opts.options) {
            if (opt.label != label) continue;

            // Group patches by file so we load each file once.
            QMap<QString, QVector<IniPatch>> byFile;
            for (const auto& patch : opt.patches) {
                const QString path = resolveConfigPath(patch.iniFilePath, adapter);
                if (path.isEmpty()) continue;
                byFile[path].append(patch);
            }

            for (auto fit = byFile.constBegin(); fit != byFile.constEnd(); ++fit) {
                IniFile ini;
                ini.load(fit.key());
                for (const auto& patch : fit.value())
                    ini.setValue(patch.section, patch.key, patch.value);
                ini.save(fit.key());
            }
            break;
        }
    }
    emit statusMessage("Aspect ratio settings saved.");
}

// ── Paths ──────────────────────────────────────────────────

QVariantList ConfigService::pathDefs(const QString& emuId) const {
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
        QString defPath;
        switch (pd.base) {
            case PathBase::Bios:
                defPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
                break;
            case PathBase::EmulatorData:
                defPath = QFileInfo(Paths::emulatorDataDir(emuId, systemId) + "/" + pd.defaultSuffix).absoluteFilePath();
                break;
        }
        item["defaultPath"] = defPath;
        list.append(item);
    }
    return list;
}

QString ConfigService::pathValue(const QString& emuId, const QString& section, const QString& key) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return {};

    IniFile ini;
    ini.load(configPath);
    return ini.value(section, key);
}

QString ConfigService::pathDefault(const QString& emuId, const QString& section, const QString& key) const {
    auto defs = pathDefs(emuId);
    for (const auto& d : defs) {
        auto map = d.toMap();
        if (map["section"].toString() == section && map["key"].toString() == key)
            return map["defaultPath"].toString();
    }
    return {};
}

void ConfigService::savePaths(const QString& emuId, const QVariantMap& values) {
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
        emit statusMessage("Paths saved.");
    else
        emit statusMessage("Failed to save paths.");
}

// ── Capture-formatting helpers ──────────────────────────────

QString ConfigService::formatCapturedBinding(const QString& emuId, int deviceIndex,
                                              const QString& element, bool isAxis, bool positive) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatBinding(deviceIndex, element, isAxis, positive);
}

QString ConfigService::formatCapturedKeyboard(const QString& emuId, int qtKey, int modifiers) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatKeyboardBinding(qtKey, modifiers);
}

QString ConfigService::formatCapturedMouse(const QString& emuId, int qtButton) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatMouseBinding(qtButton);
}

QString ConfigService::formatCapturedWheel(const QString& emuId, int direction) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->formatWheelBinding(direction);
}

// ── Hotkeys ────────────────────────────────────────────────

QVariantList ConfigService::hotkeyBindings(const QString& emuId) const {
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

void ConfigService::saveHotkey(const QString& emuId, const QString& section,
                                const QString& key, const QString& value) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    ini.setValue(section, key, value);

    if (!ini.save(configPath))
        emit statusMessage("Failed to save hotkey.");
}

void ConfigService::clearHotkey(const QString& emuId, const QString& section, const QString& key) {
    saveHotkey(emuId, section, key, "");
}

void ConfigService::resetHotkeys(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    for (const auto& def : adapter->hotkeyBindingDefs())
        ini.setValue(def.section, def.key, def.defaultValue);

    if (ini.save(configPath))
        emit statusMessage("Hotkeys reset to defaults.");
    else
        emit statusMessage("Failed to reset hotkeys.");
}

// ── Controller Types ────────────────────────────────────────

QVariantList ConfigService::controllerTypes(const QString& emuId) const {
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

QString ConfigService::controllerType(const QString& emuId, int port) const {
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

void ConfigService::setControllerType(const QString& emuId, int port, const QString& type) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = QString("Pad%1").arg(port);
    ini.setValue(section, "Type", type);

    if (ini.save(configPath))
        emit statusMessage(QString("Controller type set to %1.").arg(type));
    else
        emit statusMessage("Failed to save controller type.");
}

// ── Port-Aware Controller Bindings/Settings ─────────────────

QVariantList ConfigService::controllerBindingsForPort(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString mainConfigPath = adapter->configFilePath();
    IniFile mainIni;
    if (!mainConfigPath.isEmpty())
        mainIni.load(mainConfigPath);

    QString type = mainIni.value(QString("Pad%1").arg(port), "Type");
    if (type.isEmpty()) type = "DualShock2";

    // Bindings may live in a different file/section (e.g. PPSSPP's controls.ini).
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

QVariantList ConfigService::controllerSettingsForPort(const QString& emuId, int port) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString configPath = adapter->configFilePath();
    IniFile ini;
    if (!configPath.isEmpty())
        ini.load(configPath);

    QString type = ini.value(QString("Pad%1").arg(port), "Type");
    if (type.isEmpty()) type = "DualShock2";

    // Settings section may differ from Pad{port} (e.g. PPSSPP uses [Control]).
    QString section = adapter->controllerSettingsSection(port);

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

void ConfigService::saveBindingForPort(const QString& emuId, int port,
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
        emit statusMessage("Failed to save binding.");
}

void ConfigService::clearBindingForPort(const QString& emuId, int port, const QString& key) {
    saveBindingForPort(emuId, port, key, "");
}

void ConfigService::clearAllBindingsForPort(const QString& emuId, int port) {
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
        emit statusMessage("Bindings cleared.");
    else
        emit statusMessage("Failed to clear bindings.");
}

void ConfigService::autoMapControllerForPort(const QString& emuId, int port, int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath();
    if (configPath.isEmpty()) return;

    QString type = controllerType(emuId, port);
    QString section = adapter->controllerBindingsSection(port);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(type)) {
        // Always rewrite the SDL-0/ prefix to the requested device, even when
        // deviceIndex == 0 (no-op replace). Skipping the replace at device 0
        // was fragile — defaults that happen not to use SDL-0/ as baseline
        // would silently produce wrong bindings for device 0.
        QString mapped = def.defaultValue;
        if (!mapped.isEmpty())
            mapped.replace("SDL-0/", QString("SDL-%1/").arg(deviceIndex));
        ini.setValue(section, def.key, mapped);
    }

    if (ini.save(configPath))
        emit statusMessage("Controller auto-mapped.");
    else
        emit statusMessage("Failed to auto-map controller.");
}

void ConfigService::saveControllerSettingForPort(const QString& emuId, int port,
                                                  const QString& key, const QString& value) {
    // Controller settings (deadzone, sensitivity) live in the main config file.
    // Most emulators use Pad{port}; some (PPSSPP) use a different section.
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = adapter->controllerSettingsSection(port);
    ini.setValue(section, key, value);

    if (!ini.save(configPath))
        emit statusMessage("Failed to save controller setting.");
}

void ConfigService::restoreDefaultsForPort(const QString& emuId, int port) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString type = controllerType(emuId, port);

    QString bindingsPath = adapter->controllerBindingsConfigFilePath();
    if (!bindingsPath.isEmpty()) {
        IniFile bindingsIni;
        bindingsIni.load(bindingsPath);
        QString bindingsSection = adapter->controllerBindingsSection(port);
        for (const auto& def : adapter->controllerBindingDefsForType(type))
            bindingsIni.setValue(bindingsSection, def.key, def.defaultValue);
        bindingsIni.save(bindingsPath);
    }

    QString configPath = adapter->configFilePath();
    if (!configPath.isEmpty()) {
        IniFile ini;
        ini.load(configPath);
        QString section = adapter->controllerSettingsSection(port);
        for (const auto& def : adapter->controllerSettingDefsForType(type))
            ini.setValue(section, def.key, def.defaultValue);
        ini.save(configPath);
    }

    emit statusMessage("Controller defaults restored.");
}

// ── Controller Profiles ─────────────────────────────────────

QStringList ConfigService::controllerProfiles(const QString& emuId) const {
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

void ConfigService::createControllerProfile(const QString& emuId, const QString& name) {
    QString safeName = sanitizeProfileName(name);
    if (safeName.isEmpty()) return;

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString profileDir = Paths::configDir() + "/controller_profiles";
    QDir().mkpath(profileDir);

    QString srcPath = adapter->configFilePath();
    QString dstPath = profileDir + "/" + safeName + ".ini";

    if (srcPath.isEmpty()) return;

    IniFile src;
    src.load(srcPath);

    IniFile dst;
    for (int port = 1; port <= 2; port++) {
        QString section = QString("Pad%1").arg(port);
        for (const auto& key : src.keys(section))
            dst.setValue(section, key, src.value(section, key));
    }

    if (dst.save(dstPath))
        emit statusMessage(QString("Profile '%1' created.").arg(safeName));
    else
        emit statusMessage("Failed to create profile.");
}

void ConfigService::applyControllerProfile(const QString& emuId, const QString& name) {
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
        emit statusMessage(QString("Profile '%1' applied.").arg(name));
    else
        emit statusMessage("Failed to apply profile.");
}

void ConfigService::renameControllerProfile(const QString& emuId, const QString& oldName,
                                             const QString& newName) {
    Q_UNUSED(emuId);
    QString safeNewName = sanitizeProfileName(newName);
    if (safeNewName.isEmpty()) return;

    QString profileDir = Paths::configDir() + "/controller_profiles";
    QString oldPath = profileDir + "/" + oldName + ".ini";
    QString newPath = profileDir + "/" + safeNewName + ".ini";

    if (QFile::rename(oldPath, newPath))
        emit statusMessage(QString("Profile renamed to '%1'.").arg(safeNewName));
    else
        emit statusMessage("Failed to rename profile.");
}

void ConfigService::deleteControllerProfile(const QString& emuId, const QString& name) {
    Q_UNUSED(emuId);
    QString safeName = sanitizeProfileName(name);
    if (safeName.isEmpty()) return;

    QString profileDir = Paths::configDir() + "/controller_profiles";
    QString path = profileDir + "/" + safeName + ".ini";

    if (QFile::remove(path))
        emit statusMessage(QString("Profile '%1' deleted.").arg(safeName));
    else
        emit statusMessage("Failed to delete profile.");
}
