#pragma once

#include "adapters/emulator_adapter.h"
#include "core/libretro/core_runtime.h"
#include <QObject>
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

    // Override from Task 5.2's virtual on EmulatorAdapter.
    // NOTE: returns nullptr when no game is running (m_runtime == nullptr).
    // The settings UI will show defaults until a game has run at least once.
    // Task 8.2's GameSession::startLibretro will call prepareRuntime() early
    // enough to mitigate this v1 limitation.
    OptionsStore* libretroOptionsStore() override;

    // Used by GameSession when manifest.backend == "libretro".
    CoreRuntime* runtime() { return m_runtime.get(); }
    void prepareRuntime();
    void releaseRuntime();

    /** Per-core: e.g. "mgba". Used to compute paths under emulators/libretro/. */
    virtual QString coreId() const = 0;

    /** RA console ID for the given system. Default returns 0 (unknown).
     *  Concrete adapters override to return per-system mappings. */
    virtual int raConsoleId(const QString& systemId) const {
        Q_UNUSED(systemId); return 0;
    }

protected:
    /** Static path: {root}/emulators/libretro/cores/{core_dylib} */
    static QString coreDylibPath(const EmulatorManifest& manifest);
    /** Static path: {root}/emulators/libretro/{coreId}/options.json */
    QString optionsJsonPath() const;
    QString controlsJsonPath() const;

private:
    std::unique_ptr<CoreRuntime> m_runtime;
};
