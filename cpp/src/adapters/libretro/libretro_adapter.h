#pragma once

#include "adapters/emulator_adapter.h"
#include "core/libretro/core_runtime.h"
#include "core/libretro/declared_options.h"
#include "core/libretro/frontend_settings_store.h"
#include "core/option_overlay.h"
#include <QObject>
#include <QPair>
#include <QVector>
#include <memory>
#include <optional>

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
    /** Select the "<core>_libretro.dylib.zip" GitHub release asset. The
     *  installer's postDownload path keys off the .dylib.zip suffix to unzip
     *  into cores/ and derive the dylib name. */
    QVector<AssetMatchRule> assetMatchRules() const override;
    bool supportsRetroAchievements() const override { return true; }
    bool supportsSaveOnExit() const override { return true; }
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

    /** Reset Configuration support: delete options.json + frontend.json and
     *  drop the cached fallback stores so the next access re-seeds from
     *  defaults (core-declared + overlay overrides). No-op on the live
     *  runtime store — reset from the manage page happens outside sessions;
     *  a running game keeps its in-memory values until it exits. */
    void resetSettingsToDefaults();

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

    /**
     * Number of player ports this core supports through the libretro input
     * path. Default 1 (Player 1 only). Cores that support more return >1; the
     * host then binds the extra controllers (device indices 1..N-1) and tells
     * the core via retro_set_controller_port_device when each port gains/loses
     * a pad. Multitap is NOT modeled here.
     */
    virtual int maxLibretroPlayers() const { return 1; }

    /** Per-core: e.g. "mgba". Used to compute paths under emulators/libretro/. */
    virtual QString coreId() const = 0;

    // ── Packet 7 Stage 2: core-declared schema ──────────────────────────
    /** Curation overlay: which declared options appear in the settings UI,
     *  where, and with what presentation. See core/option_overlay.h for the
     *  merge rules. Adapters converted to the declared-schema path override
     *  this INSTEAD of settingsSchema(). */
    virtual QVector<OptionOverlay> optionOverlays() const { return {}; }

    /** Hand-authored rows PREPENDED before the merged option rows — the
     *  genuinely frontend-owned settings (Storage::FrontendSetting / Ini),
     *  e.g. mGBA's aspect_mode. Leading keeps them at the top of their
     *  category pages, matching the historical layouts. */
    virtual QVector<SettingDef> extraSettings() const { return {}; }

    /** The core's declared option table: the sidecar written by the last
     *  session, or seeded via CoreProber when none exists yet (fresh
     *  install, settings browsed before first run). nullptr when both
     *  fail. Cached for the adapter's lifetime. */
    const DeclaredOptionsDoc* declaredOptions() const;

    /** Base implementation renders optionOverlays() × declaredOptions()
     *  into SettingDef rows + extraSettings(). Adapters not yet converted
     *  keep overriding settingsSchema() directly and never hit this. */
    QVector<SettingDef> settingsSchema() const override;

    /** Test seam: inject a declared doc (skips sidecar + prober). */
    void setDeclaredDocForTest(DeclaredOptionsDoc doc) {
        m_declaredDoc = std::move(doc);
        m_declaredDocLoaded = true;
    }

    /**
     * Optional per-core override for the libretro system directory
     * (RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY). Default returns empty →
     * GameSession falls back to the shared Paths::biosDir(). Cores whose
     * CI release zip ships an asset tree next to the dylib (e.g. PPSSPP's
     * cores/ppsspp_libretro_resources/) override this to point system_dir
     * at that shipped tree. Only affects system_dir — the save/data dir
     * (GET_SAVE_DIRECTORY) is configured separately and never moves.
     */
    virtual QString systemDirOverride() const { return {}; }

    /** Default frontend setting (key, defaultValue) pairs for this core.
     *  Subclasses override to declare their frontend-managed settings.
     *  Default returns empty (no frontend settings). */
    virtual QVector<QPair<QString, QString>> frontendSettingDefaults() const {
        return {};
    }

    /**
     * Which HW render path this core uses. Drives EmulationView's Loader and
     * GameSession's m_libretroBackend selection:
     *   - None         → software (VideoSoftware → LibretroVideoItem QImage path).
     *                    mGBA + any future SW libretro core.
     *   - MetalNSView  → PCSX2's path: QQuickItem owns NSView, core draws Metal
     *                    into it via the RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW
     *                    extension. No GL/FBO involvement.
     *   - GL           → standard libretro SET_HW_RENDER (RETRO_HW_CONTEXT_OPENGL_CORE).
     *                    Frontend creates the NSOpenGL context pair + FBO via
     *                    VideoHardwareGL; LibretroGLItem imports the FBO's
     *                    IOSurface as a Metal texture for composite.
     *                    PPSSPP, future Dolphin / Citra.
     */
    enum class HardwareRenderBackend { None, MetalNSView, GL };
    virtual HardwareRenderBackend hardwareRenderBackend() const {
        return HardwareRenderBackend::None;
    }

    /**
     * Convenience predicate retained for callers that only need to know
     * whether the core wants ANY HW context. Returns true unless the
     * subclass declares HardwareRenderBackend::None.
     */
    virtual bool prefersHardwareRender() const {
        return hardwareRenderBackend() != HardwareRenderBackend::None;
    }

protected:
    /** Static path: {root}/emulators/libretro/cores/{core_dylib} */
    static QString coreDylibPath(const EmulatorManifest& manifest);
    /** Static path: {root}/emulators/libretro/{coreId}/declared_options.json */
    QString declaredOptionsSidecarPath() const;
    /** Install-path convention {root}/emulators/libretro/cores/{coreId}_libretro.dylib
     *  (matches every manifest's core_dylib) — used by the prober fallback. */
    QString coreDylibInstallPath() const;
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
    /** declaredOptions() cache (mutable: settingsSchema() is const). */
    mutable std::optional<DeclaredOptionsDoc> m_declaredDoc;
    mutable bool m_declaredDocLoaded = false;
};
