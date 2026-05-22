#include "migration_ppsspp.h"

#include "paths.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace {

constexpr const char* kEmuId = "ppsspp";
constexpr const char* kCanary = "PPSSPPSDL.app";
constexpr const char* kStandaloneMetadata = ".version.json";
constexpr const char* kUpperData = "PSP";
constexpr const char* kLowerData = "psp";
constexpr const char* kSentinelName = ".ppsspp-libretro-migrated";

bool removeRecursive(const QString& path) {
    QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) return true;
    if (info.isDir() && !info.isSymLink()) {
        if (!QDir(path).removeRecursively()) {
            qCritical() << "[MigrationPpsspp] failed to remove directory" << path;
            return false;
        }
        return true;
    }
    if (!QFile::remove(path)) {
        qCritical() << "[MigrationPpsspp] failed to remove file" << path;
        return false;
    }
    return true;
}

/** Move every entry of `from` into `to`. Overwrites on collision. Returns
 *  true on success. `from` is left empty (but not removed) on success. */
bool mergeDirInto(const QString& from, const QString& to) {
    QDir src(from);
    QDir().mkpath(to);
    const auto entries = src.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QString& name : entries) {
        const QString srcPath = src.filePath(name);
        const QString dstPath = to + "/" + name;
        if (QFileInfo(dstPath).exists()) {
            if (QFileInfo(srcPath).isDir()) {
                if (!mergeDirInto(srcPath, dstPath)) return false;
                if (!QDir(srcPath).removeRecursively()) {
                    qCritical() << "[MigrationPpsspp] failed to remove merged dir" << srcPath;
                    return false;
                }
            } else {
                // Prefer libretro's pre-existing data on collision; drop the standalone copy.
                if (!QFile::remove(srcPath)) {
                    qCritical() << "[MigrationPpsspp] failed to drop colliding file" << srcPath;
                    return false;
                }
            }
        } else {
            if (!QDir().rename(srcPath, dstPath)) {
                qCritical() << "[MigrationPpsspp] failed to move" << srcPath << "->" << dstPath;
                return false;
            }
        }
    }
    return true;
}

/** Force the on-disk PSP/ → psp/ rename via a temp name so case-insensitive
 *  volumes also flip to lowercase casing in Finder. */
bool renameToLowercase(const QString& ppspDir) {
    const QString tmp = ppspDir + "/__migration_psp_tmp";
    if (QFileInfo::exists(tmp)) {
        if (!QDir(tmp).removeRecursively()) return false;
    }
    if (!QDir(ppspDir).rename(kUpperData, "__migration_psp_tmp")) {
        qCritical() << "[MigrationPpsspp] failed to rename PSP -> temp in" << ppspDir;
        return false;
    }
    if (!QDir(ppspDir).rename("__migration_psp_tmp", kLowerData)) {
        qCritical() << "[MigrationPpsspp] failed to rename temp -> psp in" << ppspDir;
        return false;
    }
    return true;
}

bool touchFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qCritical() << "[MigrationPpsspp] failed to touch sentinel" << path;
        return false;
    }
    f.close();
    return true;
}

} // namespace

bool MigrationPpsspp::runIfNeeded() {
    const QString emusDir = Paths::emulatorsDir();
    const QString sentinel = emusDir + "/" + kSentinelName;

    if (QFile::exists(sentinel)) return true;

    QDir().mkpath(emusDir);

    const QString ppspDir = emusDir + "/" + kEmuId;
    const QString canaryPath = ppspDir + "/" + kCanary;

    if (QFileInfo::exists(canaryPath)) {
        // Standalone install detected — strip it down to just the psp/ data tree.
        if (!removeRecursive(canaryPath)) return false;
        qInfo() << "[MigrationPpsspp] removed standalone bundle" << canaryPath;

        const QString metadataPath = ppspDir + "/" + kStandaloneMetadata;
        if (QFileInfo::exists(metadataPath)) {
            if (!removeRecursive(metadataPath)) return false;
        }

        // Normalise PSP/ → psp/.
        const QDir d(ppspDir);
        const QStringList dirs = d.entryList(QStringList{kUpperData, kLowerData}, QDir::Dirs | QDir::NoDotAndDotDot);
        const bool hasUpper = dirs.contains(QString::fromUtf8(kUpperData));
        const bool hasLower = dirs.contains(QString::fromUtf8(kLowerData));
        if (hasUpper && hasLower) {
            // Case-sensitive volume with both populated — merge PSP/* into psp/, then remove PSP/.
            const QString fromDir = ppspDir + "/" + kUpperData;
            const QString toDir = ppspDir + "/" + kLowerData;
            if (!mergeDirInto(fromDir, toDir)) return false;
            if (!QDir(fromDir).removeRecursively()) {
                qCritical() << "[MigrationPpsspp] failed to remove drained PSP dir" << fromDir;
                return false;
            }
            qInfo() << "[MigrationPpsspp] merged PSP/ into psp/ in" << ppspDir;
        } else if (hasUpper) {
            if (!renameToLowercase(ppspDir)) return false;
            qInfo() << "[MigrationPpsspp] renamed PSP/ -> psp/ in" << ppspDir;
        }
        // else: only psp/ (or empty) — nothing to do.

        // Drop stale standalone INI files. libretro does not read them.
        const QString lowerPath = ppspDir + "/" + kLowerData;
        for (const char* ini : {"SYSTEM/ppsspp.ini", "SYSTEM/controls.ini"}) {
            const QString iniPath = lowerPath + "/" + ini;
            if (QFileInfo::exists(iniPath)) {
                if (!removeRecursive(iniPath)) return false;
            }
        }
    }

    return touchFile(sentinel);
}
