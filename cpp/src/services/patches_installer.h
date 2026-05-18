#pragma once

#include <QObject>
#include <QString>

/**
 * PatchesInstaller — fetches PCSX2 patches.zip from
 * github.com/PCSX2/pcsx2_patches/releases/latest into the libretro
 * core's resources directory.
 *
 * Designed for single-purpose use: PCSX2 is the only emulator in the
 * RetroNest stable that consumes an externally-released community data
 * bundle. If a second consumer ever materializes, extract a generic
 * ResourcesInstaller with the actual second use case in hand.
 */
class PatchesInstaller : public QObject {
    Q_OBJECT

public:
    /** Default age threshold for "stale" sidecar/zip (90 days). */
    static constexpr qint64 kStaleAgeSeconds = 90LL * 24 * 60 * 60;

    explicit PatchesInstaller(QObject* parent = nullptr);

    /** True if a fetch should run for the resources dir at `resourcesDir`.
     *  Pure function over the filesystem state; safe to call on main thread.
     *  Returns false if `resourcesDir` doesn't exist or its parent (the
     *  installed core dir) is missing — no point fetching patches without
     *  a core to read them. */
    bool isFetchNeeded(const QString& resourcesDir) const;

    /** Kick off the async fetch state machine on a background thread.
     *  Emits progress() during download, finished() on completion.
     *  If `force` is false and isFetchNeeded() is false, short-circuits
     *  to finished(true, "already up to date", sidecar.tag).
     *  Safe to call from the main thread. */
    void fetchAsync(const QString& resourcesDir, bool force = false);

signals:
    /** 0.0–1.0 download ratio + human-readable phase string. */
    void progress(qreal ratio, const QString& message);

    /** success=true on successful fetch OR clean short-circuit ("up to date").
     *  On failure: success=false, message is a user-facing reason. */
    void finished(bool success, const QString& message, const QString& tag);
};
