#include "paths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>

QString Paths::s_root;

bool Paths::setRoot(const QString& rootPath) {
    if (rootPath.isEmpty()) {
        qWarning() << "[Paths] setRoot called with empty path";
        return false;
    }
    if (!QDir::isAbsolutePath(rootPath)) {
        qWarning() << "[Paths] setRoot requires an absolute path, got:" << rootPath;
        return false;
    }
    s_root = rootPath;
    return true;
}

QString Paths::root() {
    return s_root;
}

QString Paths::systemIdFor(const QString& emuId, const QStringList& systems) {
    return systems.isEmpty() ? emuId : systems.first();
}

QString Paths::emulatorsDir(const QString& emuId) {
    if (emuId.isEmpty()) return s_root + "/emulators";
    return s_root + "/emulators/" + emuId;
}

QString Paths::emulatorDataDir(const QString& emuId, const QString& systemId) {
    return s_root + "/emulators/" + emuId + "/" + systemId;
}

QString Paths::biosDir() {
    return s_root + "/bios";
}

QString Paths::romsDir(const QString& systemId) {
    if (systemId.isEmpty()) return s_root + "/roms";
    return s_root + "/roms/" + systemId;
}

QString Paths::mediaDir() {
    return s_root + "/downloaded_media";
}

QString Paths::mediaDir(const QString& system) {
    return s_root + "/downloaded_media/" + system;
}

QString Paths::mediaDir(const QString& system, const QString& mediaType) {
    return s_root + "/downloaded_media/" + system + "/" + mediaType;
}

QString Paths::configDir() {
    return s_root + "/config";
}

QString Paths::themesDir() {
    return s_root + "/themes";
}

void Paths::ensureDirectories() {
    QStringList dirs = {
        s_root,
        emulatorsDir(),
        biosDir(),
        romsDir(),
        mediaDir(),
        configDir(),
        themesDir(),
    };
    for (const auto& d : dirs) {
        if (!QDir().mkpath(d))
            qWarning() << "[Paths] Failed to create directory:" << d;
    }
    qInfo() << "[Paths] Root:" << s_root;
}

void Paths::ensureRomDirectories(const QStringList& systemIds) {
    QDir().mkpath(romsDir());
    for (const auto& sys : systemIds) {
        QDir().mkpath(romsDir(sys));
    }
}

// ============================================================================
// Legacy layout migration
// ============================================================================
//
// RetroNest used to split emulator data across two top-level trees:
//   {root}/saves/{systemId}/{savestates,memcards,...}
//   {root}/data/{emuId}/{screenshots,cache,cheats,textures,...}
//
// The modern layout puts everything per-emulator under
//   {root}/emulators/{emuId}/{systemId}/<subdir>
//
// This function moves any legacy directories that still exist into their
// new home. It's safe to run on every startup: a legacy directory is only
// moved if the new target is either missing or empty (no clobbering), and
// successful moves leave behind empty parent directories that we then
// rmdir() so the old {root}/saves and {root}/data trees vanish once
// migration completes.

namespace {

// Return true if a directory exists and contains no entries (other than
// "." and ".."). Missing directories count as empty.
bool isDirEmptyOrMissing(const QString& path) {
    QDir d(path);
    if (!d.exists()) return true;
    return d.isEmpty();
}

// Move every entry inside `from` into `to`. Both paths must be
// directories. Creates `to` if it doesn't exist. Silently skips entries
// whose target already exists.
bool moveDirectoryContents(const QString& from, const QString& to) {
    QDir src(from);
    if (!src.exists()) return true;

    if (!QDir().mkpath(to)) {
        qWarning() << "[Paths] Migration: cannot create target" << to;
        return false;
    }

    const auto entries = src.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    bool allOk = true;
    for (const auto& name : entries) {
        const QString srcPath = from + "/" + name;
        const QString dstPath = to + "/" + name;
        if (QFileInfo::exists(dstPath)) {
            qWarning() << "[Paths] Migration: target already exists, skipping"
                       << srcPath << "->" << dstPath;
            allOk = false;
            continue;
        }
        if (!QDir().rename(srcPath, dstPath)) {
            qWarning() << "[Paths] Migration: failed to move" << srcPath << "->" << dstPath;
            allOk = false;
        }
    }
    return allOk;
}

// Attempt to move a single legacy directory to its new location, with the
// "new target must be empty" guard.
void migrateOne(const QString& oldPath, const QString& newPath) {
    if (!QFileInfo::exists(oldPath)) return;  // nothing to migrate
    if (!isDirEmptyOrMissing(newPath)) {
        qWarning() << "[Paths] Migration: skipping" << oldPath
                   << "— target" << newPath << "already has content";
        return;
    }
    if (moveDirectoryContents(oldPath, newPath)) {
        QDir().rmdir(oldPath);  // only succeeds if now empty
        qInfo() << "[Paths] Migrated" << oldPath << "->" << newPath;
    }
}

} // namespace

void Paths::migrateLegacyLayout() {
    if (s_root.isEmpty()) return;

    const QString r = s_root;

    struct Move { QString from; QString to; };
    const QVector<Move> moves = {
        // PCSX2 — PS2 — was split across saves/ps2 + data/pcsx2
        { r + "/saves/ps2/savestates",      r + "/emulators/pcsx2/ps2/savestates" },
        { r + "/saves/ps2/memcards",        r + "/emulators/pcsx2/ps2/memcards" },
        { r + "/saves/ps2/inputprofiles",   r + "/emulators/pcsx2/ps2/inputprofiles" },
        { r + "/data/pcsx2/screenshots",    r + "/emulators/pcsx2/ps2/screenshots" },
        { r + "/data/pcsx2/cache",          r + "/emulators/pcsx2/ps2/cache" },
        { r + "/data/pcsx2/cheats",         r + "/emulators/pcsx2/ps2/cheats" },
        { r + "/data/pcsx2/videos",         r + "/emulators/pcsx2/ps2/videos" },
        { r + "/data/pcsx2/textures",       r + "/emulators/pcsx2/ps2/textures" },
        { r + "/data/pcsx2/logs",           r + "/emulators/pcsx2/ps2/logs" },
        { r + "/data/pcsx2/patches",        r + "/emulators/pcsx2/ps2/patches" },
        { r + "/data/pcsx2/gamesettings",   r + "/emulators/pcsx2/ps2/gamesettings" },

        // DuckStation — PSX — was split across saves/psx + data/duckstation
        { r + "/saves/psx/savestates",      r + "/emulators/duckstation/psx/savestates" },
        { r + "/saves/psx/memcards",        r + "/emulators/duckstation/psx/memcards" },
        { r + "/data/duckstation/screenshots", r + "/emulators/duckstation/psx/screenshots" },
        { r + "/data/duckstation/cache",       r + "/emulators/duckstation/psx/cache" },
        { r + "/data/duckstation/cheats",      r + "/emulators/duckstation/psx/cheats" },
        { r + "/data/duckstation/textures",    r + "/emulators/duckstation/psx/textures" },
    };

    for (const auto& m : moves)
        migrateOne(m.from, m.to);

    // Clean up now-empty legacy parent directories so old trees disappear.
    const QStringList legacyRoots = {
        r + "/saves/ps2",
        r + "/saves/psx",
        r + "/saves",
        r + "/data/pcsx2",
        r + "/data/duckstation",
        r + "/data",
    };
    for (const auto& d : legacyRoots) {
        if (QFileInfo::exists(d) && isDirEmptyOrMissing(d)) {
            if (QDir().rmdir(d))
                qInfo() << "[Paths] Removed empty legacy dir" << d;
        }
    }
}

QString Paths::appConfigPath() {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return appData + "/config.json";
}

QString Paths::loadSavedRoot() {
    QFile f(appConfigPath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    return obj["root"].toString();
}

void Paths::saveRoot(const QString& rootPath) {
    QJsonObject obj;
    obj["root"] = rootPath;

    QFile f(appConfigPath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson());
        f.close();
    }
    qInfo() << "[Paths] Saved root to" << appConfigPath();
}
