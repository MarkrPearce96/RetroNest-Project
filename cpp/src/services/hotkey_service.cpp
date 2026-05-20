#include "hotkey_service.h"

#include "adapters/adapter_registry.h"
#include "core/ini_file.h"
#include "core/libretro/libretro_hotkey_defs.h"

#include <QDir>
#include <QStandardPaths>

// Fixed storage path for the global libretro hotkey INI. The parent directory
// is created on demand so first-run saves don't fail with a missing directory.
static QString libretroHotkeysIniPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/libretro_hotkeys.ini");
}

HotkeyService::HotkeyService(QObject* parent) : QObject(parent) {}

QVariantList HotkeyService::hotkeyBindings(const QString& emuId) const {
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

void HotkeyService::saveHotkey(const QString& emuId, const QString& section,
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

void HotkeyService::clearHotkey(const QString& emuId, const QString& section, const QString& key) {
    saveHotkey(emuId, section, key, "");
}

void HotkeyService::resetHotkeys(const QString& emuId) {
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
