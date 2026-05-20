#pragma once

#include "core/manifest_loader.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

class SdlInputManager;
class HotkeyService;

class IniFile;

// ConfigService — owns all per-emulator settings/path/binding/profile
// orchestration that used to live inline in AppController. This includes
// the per-dialog INI cache (held while a settings dialog is open).
//
// Hotkeys live in HotkeyService (sibling); ConfigService keeps a non-owning
// pointer for the resetConfiguration cross-tie only.
//
// Methods are called from AppController's Q_INVOKABLE shims; signals are
// forwarded through AppController out to QML.
class ConfigService : public QObject {
    Q_OBJECT
public:
    explicit ConfigService(ManifestLoader* loader,
                           HotkeyService* hotkeyService,
                           QObject* parent = nullptr);
    void setSdlInputManager(SdlInputManager* mgr) { m_inputManager = mgr; }
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
    void resetPaths(const QString& emuId);

    // Controller types
    QVariantList controllerTypes(const QString& emuId) const;
    QString controllerType(const QString& emuId, int port) const;
    void setControllerType(const QString& emuId, int port, const QString& type);

    // Controller bindings (port-aware)
    QVariantList controllerBindingsForPort(const QString& emuId, int port,
                                            const QString& controllerTypeId) const;
    void saveBindingForPort(const QString& emuId, int port,
                             const QString& controllerTypeId,
                             const QString& key, const QString& value,
                             int deviceIndex = -1);
    void clearBindingForPort(const QString& emuId, int port,
                              const QString& controllerTypeId,
                              const QString& key);
    void clearAllBindingsForPort(const QString& emuId, int port,
                                  const QString& controllerTypeId);
    void autoMapControllerForPort(const QString& emuId, int port,
                                   const QString& controllerTypeId,
                                   int deviceIndex);
    void restoreDefaultsForPort(const QString& emuId, int port);

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
    SdlInputManager* m_inputManager = nullptr;
    HotkeyService* m_hotkeyService;   // non-owning; owned by AppController

    // Settings cache — single-entry, since only one settings dialog is open
    // at a time. Populated by beginSettingsSession, dropped by end.
    mutable std::unique_ptr<IniFile> m_settingsCache;
    QString m_settingsCachePath;
};
