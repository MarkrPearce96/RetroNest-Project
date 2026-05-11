#pragma once

#include "adapters/emulator_adapter.h"
#include "core/libretro/core_runtime.h"
#include "core/libretro/frontend_settings_store.h"
#include <QObject>
#include <QPair>
#include <QVector>
#include <memory>

class LibretroAdapter : public QObject, public EmulatorAdapter {
    Q_OBJECT
public:
    LibretroAdapter() = default;
    ~LibretroAdapter() override = default;

    // EmulatorAdapter
    bool ensureConfig(const EmulatorManifest& manifest,
                      const QString& biosPath,
                      const QString& savesPath) override;
    QString resolveExecutable(const EmulatorManifest& manifest,
                              const QString& installPath) override;
    bool isInstalled(const EmulatorManifest& manifest) override;
    DirectDownloadInfo resolveDirectDownload(const EmulatorManifest& manifest) const override;
    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return true; }
    QStringList resumeLaunchArgs(const QString&) const override { return {}; }
    QString findResumeFile(const QString& serial) const override;

    /** Push RA pref changes into the live RcheevosRuntime instead of INI-patching. */
    void patchRetroAchievements(const QString& username, const QString& token,
                                bool enabled, bool hardcore,
                                bool notifications, bool sounds) override;

    /** Typed downcast — overrides EmulatorAdapter's nullptr default. */
    LibretroAdapter* asLibretro() override { return this; }

    // Returns the live runtime's OptionsStore when a game is running; otherwise
    // a persistent fallback store owned by this adapter, lazily loaded from
    // optionsJsonPath() and seeded with declared options synthesized from
    // settingsSchema(). The fallback lets the settings dialog read & write
    // libretro options without requiring a game to have run first — both
    // stores ultimately persist to the same options.json on disk, so the
    // runtime sees the user's edits on next launch.
    virtual OptionsStore* libretroOptionsStore();

    // Return the frontend settings store for this adapter.
    // Lazily initialised on first call; loads from frontend.json using the
    // defaults returned by frontendSettingDefaults(). Never returns nullptr
    // after the first call (unlike libretroOptionsStore which requires a live
    // runtime — frontend settings are independent of the core thread).
    virtual FrontendSettingsStore* frontendSettingsStore();

    // Used by GameSession when manifest.backend == "libretro".
    CoreRuntime* runtime() { return m_runtime.get(); }
    void prepareRuntime();
    void releaseRuntime();

    // Controller-mapping INI: {root}/emulators/libretro/{coreId}/controls.ini
    // These overrides make ConfigService::saveBindingForPort / controllerBindingsForPort
    // work transparently for all libretro adapters.
    QString controllerBindingsConfigFilePath() const override;
    QString controllerBindingsSection(int port) const override;
    QString controllerBindingsConfigFilePath(const QString& controllerTypeId) const override;
    QString controllerBindingsSection(int port, const QString& controllerTypeId) const override;

    /** Per-core: e.g. "mgba". Used to compute paths under emulators/libretro/. */
    virtual QString coreId() const = 0;

    /** RA console ID for the given system. Default returns 0 (unknown).
     *  Concrete adapters override to return per-system mappings. */
    virtual int raConsoleId(const QString& systemId) const {
        Q_UNUSED(systemId); return 0;
    }

    /** Default frontend setting (key, defaultValue) pairs for this core.
     *  Subclasses override to declare their frontend-managed settings.
     *  Default returns empty (no frontend settings). */
    virtual QVector<QPair<QString, QString>> frontendSettingDefaults() const {
        return {};
    }

    /**
     * True if this core requires a hardware-rendering host context (CAMetalLayer
     * on macOS). When true, EmulationView hosts LibretroMetalItem and registers
     * its NSView with CoreRuntime before retro_load_game. When false (default),
     * EmulationView hosts LibretroVideoItem and the core renders in software.
     */
    virtual bool prefersHardwareRender() const { return false; }

protected:
    /** Static path: {root}/emulators/libretro/cores/{core_dylib} */
    static QString coreDylibPath(const EmulatorManifest& manifest);
    /** Static path: {root}/emulators/libretro/{coreId}/options.json */
    QString optionsJsonPath() const;
    /** Static path: {root}/emulators/libretro/{coreId}/controls.ini */
    QString controlsIniPath() const;
    /** Static path: {root}/emulators/libretro/{coreId}/frontend.json */
    QString frontendJsonPath() const;

private:
    std::unique_ptr<CoreRuntime> m_runtime;
    std::unique_ptr<FrontendSettingsStore> m_frontendSettings;
    /** Fallback OptionsStore used when m_runtime is null. Loaded from
     *  optionsJsonPath() with declared options synthesized from
     *  settingsSchema(). Reset whenever m_runtime is created or destroyed
     *  so the next no-runtime access reloads fresh values from disk. */
    std::unique_ptr<OptionsStore> m_persistentOptions;
};
