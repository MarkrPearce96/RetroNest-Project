#pragma once

#include "core/manifest_loader.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

class IniFile;

// ConfigService — owns all per-emulator settings/path/hotkey/binding/profile
// orchestration that used to live inline in AppController. This includes
// the per-dialog INI cache (held while a settings dialog is open).
//
// Methods are called from AppController's Q_INVOKABLE shims; signals are
// forwarded through AppController out to QML.
class ConfigService : public QObject {
    Q_OBJECT
public:
    explicit ConfigService(ManifestLoader* loader, QObject* parent = nullptr);
    ~ConfigService() override;

    // Settings (per-emulator)
    QVariantList settingsSchema(const QString& emuId) const;
    QString settingValue(const QString& emuId, const QString& section, const QString& key) const;
    void saveSettings(const QString& emuId, const QVariantMap& values);
    void resetConfiguration(const QString& emuId);

    // Settings session lifecycle (called by settings dialog ctor/dtor).
    void beginSettingsSession(const QString& emuId);
    void endSettingsSession(const QString& emuId);

    // Quick settings
    QVariantList quickResolutionOptions(const QString& emuId) const;
    QString currentResolution(const QString& emuId) const;
    void applyQuickResolution(const QVariantMap& choices);
    QVariantList quickAspectRatioOptions(const QString& emuId) const;
    QString currentAspectRatio(const QString& emuId) const;
    void applyQuickAspectRatio(const QVariantMap& choices);

    // Paths
    QVariantList pathDefs(const QString& emuId) const;
    QString pathValue(const QString& emuId, const QString& section, const QString& key) const;
    QString pathDefault(const QString& emuId, const QString& section, const QString& key) const;
    void savePaths(const QString& emuId, const QVariantMap& values);

    // Hotkeys
    QVariantList hotkeyBindings(const QString& emuId) const;
    void saveHotkey(const QString& emuId, const QString& section,
                    const QString& key, const QString& value);
    void clearHotkey(const QString& emuId, const QString& section, const QString& key);
    void resetHotkeys(const QString& emuId);

    // Controller types
    QVariantList controllerTypes(const QString& emuId) const;
    QString controllerType(const QString& emuId, int port) const;
    void setControllerType(const QString& emuId, int port, const QString& type);

    // Controller bindings + settings (port-aware)
    QVariantList controllerBindingsForPort(const QString& emuId, int port) const;
    QVariantList controllerSettingsForPort(const QString& emuId, int port) const;
    void saveBindingForPort(const QString& emuId, int port,
                             const QString& key, const QString& value);
    void clearBindingForPort(const QString& emuId, int port, const QString& key);
    void clearAllBindingsForPort(const QString& emuId, int port);
    void autoMapControllerForPort(const QString& emuId, int port, int deviceIndex);
    void saveControllerSettingForPort(const QString& emuId, int port,
                                       const QString& key, const QString& value);
    void restoreDefaultsForPort(const QString& emuId, int port);

    // Controller profiles
    QStringList controllerProfiles(const QString& emuId) const;
    void createControllerProfile(const QString& emuId, const QString& name);
    void applyControllerProfile(const QString& emuId, const QString& name);
    void renameControllerProfile(const QString& emuId, const QString& oldName,
                                  const QString& newName);
    void deleteControllerProfile(const QString& emuId, const QString& name);

    // Capture-formatting helpers (delegated to adapter)
    QString formatCapturedBinding(const QString& emuId, int deviceIndex,
                                   const QString& element, bool isAxis, bool positive) const;
    QString formatCapturedKeyboard(const QString& emuId, int qtKey, int modifiers) const;
    QString formatCapturedMouse(const QString& emuId, int qtButton) const;
    QString formatCapturedWheel(const QString& emuId, int direction) const;

signals:
    void statusMessage(const QString& msg);
    void configurationReset(const QString& emuId);

private:
    ManifestLoader* m_loader;

    // Settings cache — single-entry, since only one settings dialog is open
    // at a time. Populated by beginSettingsSession, dropped by end.
    mutable std::unique_ptr<IniFile> m_settingsCache;
    QString m_settingsCachePath;
};
