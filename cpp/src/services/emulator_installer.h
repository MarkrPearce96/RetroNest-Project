#pragma once

#include "core/manifest.h"
#include <QObject>
#include <QString>

/**
 * EmulatorInstaller — downloads an emulator from GitHub releases and extracts it.
 *
 * Supports both synchronous (CLI) and asynchronous (GUI) install flows.
 * Async mode emits progress signals for download byte-level tracking
 * and extraction phase changes.
 */
class EmulatorInstaller : public QObject {
    Q_OBJECT

public:
    struct InstallResult {
        bool success = false;
        QString message;
        QString version;      // GitHub tag_name (display)
        QString publishedAt;  // GitHub published_at — unique per release, used for update checks
    };

    explicit EmulatorInstaller(QObject* parent = nullptr);

    /** Async install — emits progress() during download/extract, finished() when done. */
    void installAsync(const EmulatorManifest& manifest, const QString& installPath);

    /** Synchronous install — blocks until complete. For CLI mode. */
    static InstallResult installSync(const EmulatorManifest& manifest, const QString& installPath);

signals:
    /** Emitted during install phases.
     *  @param ratio   0.0–1.0 for download, -1 for indeterminate (extracting)
     *  @param phase   "Fetching", "Downloading", or "Extracting"
     *  @param detail  Human-readable detail, e.g. "62% — 48 MB / 78 MB"
     */
    void progress(double ratio, const QString& phase, const QString& detail);

    /** Emitted when the install completes (success or failure). */
    void finished(EmulatorInstaller::InstallResult result);

private:
    /** Pick the right asset name from a GitHub release for this platform. */
    static QString matchAsset(const QString& emuId, const QStringList& assetNames);

    /** Extract an archive to a destination directory. Returns true on success. */
    static bool extract(const QString& archivePath, const QString& destPath);

    // Internal helpers used by both sync and async paths
    struct ReleaseInfo {
        bool ok = false;
        QString errorMessage;
        QString tagName;
        QString publishedAt;
        QString assetName;
        QString downloadUrl;
    };

    static ReleaseInfo fetchReleaseInfo(const EmulatorManifest& manifest);
    static InstallResult postDownload(const QString& tempFile,
                                       const QString& installPath,
                                       const QString& tagName,
                                       const QString& publishedAt);
};
