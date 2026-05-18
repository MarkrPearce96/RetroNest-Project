#include "migration_pcsx2.h"

#include "paths.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

bool looksLikeStandaloneInstall(const QString& dir) {
    const QDir d(dir);
    if (!d.exists()) return false;
    if (d.exists("portable.txt")) return true;
    if (d.exists(".version.json")) return true;
    if (d.exists("inis")) return true;
    if (d.exists("resources")) return true;
    const auto apps = d.entryList(QStringList() << "PCSX2-v*.app", QDir::Dirs);
    if (!apps.isEmpty()) return true;
    return false;
}

bool moveDir(const QString& from, const QString& to) {
    if (!QDir().rename(from, to)) {
        qCritical() << "[MigrationPcsx2] failed to move" << from << "->" << to;
        return false;
    }
    return true;
}

bool touchFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qCritical() << "[MigrationPcsx2] failed to touch sentinel" << path;
        return false;
    }
    f.close();
    return true;
}

} // namespace

bool MigrationPcsx2::runIfNeeded() {
    const QString emusDir = Paths::emulatorsDir();
    const QString sentinel = emusDir + "/.sp8-migrated";

    if (QFile::exists(sentinel)) return true;

    QDir().mkpath(emusDir);

    const QString standaloneDir = emusDir + "/pcsx2";
    const QString libretroDir   = emusDir + "/pcsx2-libretro";

    // Step 1: archive standalone install if present
    if (looksLikeStandaloneInstall(standaloneDir)) {
        const QString archiveRoot = emusDir + "/.archive";
        QDir().mkpath(archiveRoot);
        const QString ts = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss");
        const QString archiveTarget = archiveRoot + "/pcsx2-standalone-" + ts;
        if (!moveDir(standaloneDir, archiveTarget)) return false;
        qInfo() << "[MigrationPcsx2] archived standalone install to" << archiveTarget;
    } else if (QDir(standaloneDir).exists()) {
        // emulators/pcsx2/ exists but isn't a standalone install — could
        // be pre-migrated state. Leave it alone; the promote step below
        // bails if there's a collision.
        qInfo() << "[MigrationPcsx2] emulators/pcsx2/ exists but doesn't look standalone — leaving untouched";
    }

    // Step 2: promote libretro data
    if (QDir(libretroDir).exists()) {
        if (QDir(standaloneDir).exists()) {
            qCritical() << "[MigrationPcsx2] cannot promote libretro: emulators/pcsx2/ still exists after archive step";
            return false;
        }
        if (!moveDir(libretroDir, standaloneDir)) return false;
        qInfo() << "[MigrationPcsx2] promoted libretro data to" << standaloneDir;
    }

    // Step 3: sentinel
    return touchFile(sentinel);
}
