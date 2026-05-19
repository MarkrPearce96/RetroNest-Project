#include "config_service.h"

#include "adapters/adapter_registry.h"
#include "core/ini_file.h"
#include "core/libretro/libretro_hotkey_defs.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"
#include "core/setting_def.h"
#include "core/sdl_input_manager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <memory>
#include <vector>


// Returns iniFilePath if non-empty, else adapter->configFilePath().
// Used by quick-settings paths so adapters can target a non-main INI file
// (e.g. Dolphin's GFX.ini for resolution/aspect).
static QString resolveConfigPath(const QString& iniFilePath, EmulatorAdapter* adapter) {
    return iniFilePath.isEmpty() ? adapter->configFilePath() : iniFilePath;
}

// Fixed storage path for the global libretro hotkey INI. The parent directory
// is created on demand so first-run saves don't fail with a missing directory.
static QString libretroHotkeysIniPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/libretro_hotkeys.ini");
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

    // Resolve which file to read: per-SettingDef override if set, else
    // adapter->configFilePath().
    QString configPath;
    for (const auto& d : adapter->settingsSchema()) {
        if (d.section == section && d.key == key) {
            configPath = d.iniFilePath;
            break;
        }
    }
    if (configPath.isEmpty()) configPath = adapter->configFilePath();
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

    const QString defaultPath = adapter->configFilePath();
    if (defaultPath.isEmpty()) return;

    // Build a per-key file map from the schema — empty iniFilePath means
    // "use defaultPath".
    QHash<QPair<QString,QString>, QString> fileForKey;
    for (const auto& d : adapter->settingsSchema()) {
        if (!d.iniFilePath.isEmpty())
            fileForKey.insert({d.section, d.key}, d.iniFilePath);
    }

    // Group incoming values by destination file.
    QHash<QString, QMap<QString, QString>> writesByFile;  // file → "section/key" → value
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const int lastSlash = it.key().lastIndexOf('/');
        if (lastSlash <= 0) {
            qWarning() << "[Settings] Skipping malformed key:" << it.key();
            continue;
        }
        const QString section = it.key().left(lastSlash);
        const QString key     = it.key().mid(lastSlash + 1);
        const QString path    = fileForKey.value({section, key}, defaultPath);
        writesByFile[path].insert(section + "/" + key, it.value().toString());
    }

    // Two-phase commit across files:
    //   Phase 1: load each file, apply its patches, write to a QSaveFile temp.
    //   Phase 2: only if every Phase-1 write succeeds, commit() each in turn.
    // A failed Phase-1 cancels every QSaveFile (their temps are deleted) and
    // leaves all originals untouched. Phase-2 failure is logged per file —
    // it can still leave a partial commit on disk if the OS fails midway, but
    // multi-file rename-after-write is the closest practical approach to
    // multi-file transactional writes on commodity filesystems.
    struct PendingWrite {
        QString path;
        std::unique_ptr<QSaveFile> saver;
    };
    std::vector<PendingWrite> pending;
    pending.reserve(writesByFile.size());
    bool phase1Ok = true;

    for (auto it = writesByFile.constBegin(); it != writesByFile.constEnd(); ++it) {
        const QString& path = it.key();
        const auto& entries = it.value();

        const bool useCache = (m_settingsCache && m_settingsCachePath == path);
        IniFile localIni;
        IniFile& ini = useCache ? *m_settingsCache : localIni;
        if (!useCache) ini.load(path);

        for (auto e = entries.constBegin(); e != entries.constEnd(); ++e) {
            const int lastSlash = e.key().lastIndexOf('/');
            const QString section = e.key().left(lastSlash);
            const QString key     = e.key().mid(lastSlash + 1);
            ini.setValue(section, key, e.value());
        }

        auto saver = std::make_unique<QSaveFile>(path);
        if (!saver->open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[Settings] Phase-1 open failed for" << path;
            phase1Ok = false;
            break;
        }
        const QByteArray bytes = ini.serialize().toUtf8();
        if (saver->write(bytes) != bytes.size()) {
            qWarning() << "[Settings] Phase-1 write failed for" << path;
            saver->cancelWriting();
            phase1Ok = false;
            break;
        }
        pending.push_back({path, std::move(saver)});
    }

    if (!phase1Ok) {
        for (auto& p : pending)
            p.saver->cancelWriting();
        emit statusMessage("Failed to save settings (no changes written).");
        return;
    }

    bool commitOk = true;
    for (auto& p : pending) {
        if (!p.saver->commit()) {
            qWarning() << "[Settings] Phase-2 commit failed for" << p.path;
            commitOk = false;
        }
    }

    emit statusMessage(commitOk ? "Settings saved."
                                : "Failed to save some settings (partial commit).");
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
    if (configPath.isEmpty()) {
        // Libretro adapter — no INI. Read from PathOverridesStore.
        // `section` is informational ("libretro") and ignored; the key
        // alone scopes the override within the emulator namespace.
        return PathOverridesStore::instance().read(emuId, key);
    }

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
    if (configPath.isEmpty()) {
        // Libretro adapter — route to PathOverridesStore.
        auto& store = PathOverridesStore::instance();
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            const int lastSlash = it.key().lastIndexOf('/');
            if (lastSlash <= 0) {
                qWarning() << "[Paths] Skipping malformed key (no section separator):" << it.key();
                continue;
            }
            const QString key = it.key().mid(lastSlash + 1);
            store.write(emuId, key, it.value().toString());
        }
        // PathOverridesStore::write logs qWarning on disk failure but returns void;
        // we optimistically report success here. A future improvement could surface
        // write errors via a return value.
        emit statusMessage("Paths saved.");
        return;
    }

    IniFile ini;
    ini.load(configPath);
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const int lastSlash = it.key().lastIndexOf('/');
        if (lastSlash <= 0) {
            qWarning() << "[Paths] Skipping malformed key (no section separator):" << it.key();
            continue;
        }
        const QString section = it.key().left(lastSlash);
        const QString key     = it.key().mid(lastSlash + 1);
        ini.setValue(section, key, it.value().toString());
    }

    if (ini.save(configPath))
        emit statusMessage("Paths saved.");
    else
        emit statusMessage("Failed to save paths.");
}

void ConfigService::resetPaths(const QString& emuId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const QString configPath = adapter->configFilePath();
    if (configPath.isEmpty()) {
        // Libretro adapter — clear every overridable key from
        // PathOverridesStore. The Paths UI text field is bound to
        // `pathValue() || defaultPath`, so a cleared override makes
        // the field display the default path. Also future-proof:
        // if the user later relocates ~/Documents/RetroNest the
        // resolved default updates automatically (no stale absolute
        // path frozen in the store).
        auto& store = PathOverridesStore::instance();
        for (const auto& pd : adapter->pathsDefs())
            store.clear(emuId, pd.key);
        emit statusMessage("Paths reset to defaults.");
        return;
    }

    // Native adapter — INI is the source of truth for path values
    // the external emulator process reads on launch. Clearing keys
    // would let the emulator fall back to its own built-in defaults
    // (which often differ from RetroNest's `defaultSuffix`-derived
    // paths). Write the RetroNest-managed defaults explicitly so
    // the post-reset state matches what `pathDefs()` advertises.
    const auto* manifest = m_loader->emulatorById(emuId);
    const QString systemId = manifest ? Paths::systemIdFor(emuId, manifest->systems) : emuId;
    QVariantMap values;
    for (const auto& pd : adapter->pathsDefs()) {
        QString defPath;
        switch (pd.base) {
            case PathBase::Bios:
                defPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
                break;
            case PathBase::EmulatorData:
                defPath = QFileInfo(Paths::emulatorDataDir(emuId, systemId) + "/" + pd.defaultSuffix).absoluteFilePath();
                break;
        }
        values[pd.section + "/" + pd.key] = defPath;
    }
    savePaths(emuId, values);
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
    if (emuId == libretro_hotkeys::kSentinelEmuId) {
        IniFile ini;
        ini.load(libretroHotkeysIniPath());
        QVariantList list;
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys) {
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
    if (emuId == libretro_hotkeys::kSentinelEmuId) {
        const QString configPath = libretroHotkeysIniPath();
        IniFile ini;
        ini.load(configPath);
        ini.setValue(section, key, value);
        if (!ini.save(configPath))
            emit statusMessage("Failed to save hotkey.");
        return;
    }

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
    if (emuId == libretro_hotkeys::kSentinelEmuId) {
        const QString configPath = libretroHotkeysIniPath();
        IniFile ini;
        ini.load(configPath);
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys)
            ini.setValue(def.section, def.key, def.defaultValue);
        if (ini.save(configPath))
            emit statusMessage("Hotkeys reset to defaults.");
        else
            emit statusMessage("Failed to reset hotkeys.");
        return;
    }

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
    if (!type.isEmpty()) return type;

    // INI hasn't been patched yet (e.g. user opened the binding dialog before
    // the first game launch). Fall back to the adapter's primary type instead
    // of a hard-coded "DualShock2" so DuckStation/PPSSPP/etc. don't return an
    // empty bindings list and render every card as "Not bound".
    const auto types = adapter->controllerTypes();
    return types.isEmpty() ? QString("DualShock2") : types.first().id;
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

QVariantList ConfigService::controllerBindingsForPort(const QString& emuId, int port,
                                                       const QString& controllerTypeId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};

    QString bindingsPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    IniFile bindingsIni;
    if (!bindingsPath.isEmpty())
        bindingsIni.load(bindingsPath);

    QString section = adapter->controllerBindingsSection(port, controllerTypeId);

    QVariantList list;
    for (const auto& def : adapter->controllerBindingDefsForType(controllerTypeId)) {
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



void ConfigService::saveBindingForPort(const QString& emuId, int port,
                                        const QString& controllerTypeId,
                                        const QString& key, const QString& value,
                                        int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    if (configPath.isEmpty()) return;

    IniFile ini;
    ini.load(configPath);
    QString section = adapter->controllerBindingsSection(port, controllerTypeId);
    ini.setValue(section, key, value);

    if (deviceIndex >= 0)
        adapter->writeBindingDeviceHeader(ini, section, deviceIndex, m_inputManager);

    if (!ini.save(configPath))
        emit statusMessage("Failed to save binding.");
}

void ConfigService::clearBindingForPort(const QString& emuId, int port,
                                         const QString& controllerTypeId,
                                         const QString& key) {
    saveBindingForPort(emuId, port, controllerTypeId, key, "", /*deviceIndex=*/-1);
}

void ConfigService::clearAllBindingsForPort(const QString& emuId, int port,
                                             const QString& controllerTypeId) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    if (configPath.isEmpty()) return;

    QString section = adapter->controllerBindingsSection(port, controllerTypeId);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(controllerTypeId))
        ini.setValue(section, def.key, "");

    if (ini.save(configPath))
        emit statusMessage("Bindings cleared.");
    else
        emit statusMessage("Failed to clear bindings.");
}

void ConfigService::autoMapControllerForPort(const QString& emuId, int port,
                                              const QString& controllerTypeId,
                                              int deviceIndex) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    QString configPath = adapter->controllerBindingsConfigFilePath(controllerTypeId);
    if (configPath.isEmpty()) return;

    QString section = adapter->controllerBindingsSection(port, controllerTypeId);

    IniFile ini;
    ini.load(configPath);

    for (const auto& def : adapter->controllerBindingDefsForType(controllerTypeId)) {
        QString mapped = def.defaultValue;
        if (!mapped.isEmpty())
            mapped.replace("SDL-0/", QString("SDL-%1/").arg(deviceIndex));
        ini.setValue(section, def.key, mapped);
    }

    adapter->writeBindingDeviceHeader(ini, section, deviceIndex, m_inputManager);

    if (ini.save(configPath))
        emit statusMessage("Controller auto-mapped.");
    else
        emit statusMessage("Failed to auto-map controller.");
}

void ConfigService::restoreDefaultsForPort(const QString& emuId, int port) {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return;

    const auto types = adapter->controllerTypes();

    for (const auto& t : types) {
        QString bindingsPath = adapter->controllerBindingsConfigFilePath(t.id);
        if (bindingsPath.isEmpty()) continue;
        IniFile bindingsIni;
        bindingsIni.load(bindingsPath);
        QString bindingsSection = adapter->controllerBindingsSection(port, t.id);
        for (const auto& def : adapter->controllerBindingDefsForType(t.id))
            bindingsIni.setValue(bindingsSection, def.key, def.defaultValue);
        bindingsIni.save(bindingsPath);
    }

    QString configPath = adapter->configFilePath();
    if (!configPath.isEmpty()) {
        IniFile ini;
        ini.load(configPath);
        QString section = adapter->controllerSettingsSection(port);
        QString type = controllerType(emuId, port);
        for (const auto& def : adapter->controllerSettingDefsForType(type))
            ini.setValue(section, def.key, def.defaultValue);
        ini.save(configPath);
    }

    emit statusMessage("Controller defaults restored.");
}


