#pragma once

#include "core/manifest_loader.h"
#include "core/emulator_installer.h"
#include <QObject>
#include <QString>

/**
 * EmulatorService — orchestrates install, uninstall, and config workflows.
 *
 * Provides both synchronous (CLI) and asynchronous (GUI) install paths,
 * and tracks installed emulator versions via .version.json files.
 */
class EmulatorService : public QObject {
    Q_OBJECT

public:
    using InstallResult = EmulatorInstaller::InstallResult;

    explicit EmulatorService(ManifestLoader* loader, QObject* parent = nullptr);

    /** Synchronous install — blocks until complete. For CLI mode. */
    InstallResult installEmulatorSync(const QString& emuId);

    /** Async install — emits installProgress/installFinished. For GUI mode. */
    void installEmulatorAsync(const QString& emuId);

    /** Async uninstall — removes install directory, emits uninstallFinished. */
    void uninstallEmulator(const QString& emuId);

    /** Check GitHub for updates to installed emulators (async, rate-limited to once/day). */
    void checkForUpdates();

    /** Read installed version from .version.json. Returns empty string if not found. */
    QString installedVersion(const QString& emuId) const;

    /** Write version info to .version.json. */
    void saveVersion(const QString& emuId, const QString& version);

signals:
    void statusMessage(const QString& msg);
    void installProgress(const QString& emuId, double progress,
                         const QString& phase, const QString& detail);
    void installFinished(const QString& emuId, bool success, const QString& message);
    void uninstallFinished(const QString& emuId, bool success, const QString& message);
    void updateAvailable(const QString& emuId, const QString& currentVersion,
                         const QString& latestVersion);

private:
    /** Run ensureConfig for the given emulator. */
    void ensureConfig(const QString& emuId, const EmulatorManifest& manifest);

    ManifestLoader* m_loader;
};
