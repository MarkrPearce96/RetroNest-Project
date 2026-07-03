#pragma once

#include "core/manifest_loader.h"
#include "emulator_installer.h"
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>

/**
 * EmulatorService — orchestrates install, uninstall, and config workflows.
 *
 * Provides both synchronous (CLI) and asynchronous (GUI) install paths,
 * and tracks installed emulator versions. Process-backend emulators use a
 * `.version.json` in their private install folder; libretro cores share
 * one install folder, so each core gets a per-dylib JSON sidecar
 * `cores/<core_dylib>.version` (otherwise installing core A would stamp
 * core B's version and corrupt update detection suite-wide).
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

    /** Read installed version tag from the emulator's version record. Returns empty string if not found. */
    QString installedVersion(const QString& emuId) const;

    /** Read installed release's GitHub published_at from the version record. Empty for legacy installs. */
    QString installedPublishedAt(const QString& emuId) const;

    /** Read when the current install was performed. ISO 8601, or empty if not recorded. */
    QString installedAt(const QString& emuId) const;

    /** Write the version record. publishedAt is the GitHub release's published_at (unique per release). */
    void saveVersion(const QString& emuId, const QString& version, const QString& publishedAt);

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

    /**
     * Load the version record for an emulator (keys: version, published_at,
     * installed_at). Libretro cores read the per-dylib sidecar; if that is
     * missing or predates the JSON format, falls back to the legacy shared
     * `.version.json` (qWarning once per core) so pre-migration installs
     * don't all show as "not installed". The sidecar is written on the next
     * install, after which the legacy file is ignored for that core.
     */
    QJsonObject readVersionRecord(const QString& emuId) const;

    ManifestLoader* m_loader;

    /** emuIds already warned about the legacy version-record fallback. */
    mutable QSet<QString> m_legacyVersionWarned;
};
