#include "patches_installer.h"
#include "patches_sidecar.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

PatchesInstaller::PatchesInstaller(QObject* parent) : QObject(parent) {}

bool PatchesInstaller::isFetchNeeded(const QString& resourcesDir) const {
    if (resourcesDir.isEmpty()) return false;
    const QFileInfo dirInfo(resourcesDir);
    if (!dirInfo.exists()) return false;

    const QString zipPath = resourcesDir + "/patches.zip";
    const QString sidecarPath = zipPath + ".version";

    const QFileInfo zipInfo(zipPath);
    if (!zipInfo.exists()) return true;  // zip missing → fetch

    const auto sidecar = PatchesSidecar::read(sidecarPath);
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (sidecar.has_value() && !sidecar->installedAt.isEmpty()) {
        const QDateTime installed = QDateTime::fromString(
            sidecar->installedAt, Qt::ISODate);
        if (!installed.isValid()) return true;  // malformed = treat as stale
        return installed.secsTo(now) > kStaleAgeSeconds;
    }

    // Zip present, sidecar absent or unusable → user-placed file.
    // Respect it until zip mtime crosses staleness threshold.
    const QDateTime zipMtime = zipInfo.lastModified().toUTC();
    return zipMtime.secsTo(now) > kStaleAgeSeconds;
}

void PatchesInstaller::fetchAsync(const QString& resourcesDir, bool force) {
    // Implemented in Task 4. Stub for now so this file compiles.
    Q_UNUSED(resourcesDir);
    Q_UNUSED(force);
    emit finished(false, "fetchAsync not yet implemented", {});
}
