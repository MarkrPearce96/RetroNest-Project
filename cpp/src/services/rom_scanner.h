#pragma once

#include "core/manifest_loader.h"
#include "core/database.h"
#include <QString>
#include <QSet>

/**
 * RomScanner — scans a directory for ROM files, matches them to systems
 * using manifest rom_extensions, and inserts them into the database.
 */
class RomScanner {
public:
    struct Result {
        int added = 0;
        int skipped = 0;  // already in DB
    };

    /** Scan a directory recursively and import matching ROMs.
     *  If systemFilter is non-empty, only import ROMs for that system. */
    static Result scan(const QString& directory,
                       const ManifestLoader& loader,
                       Database& db,
                       const QString& systemFilter = {});

    /** Scan per-system subdirectories under romsBaseDir (ES-DE style).
     *  Each subfolder name is matched to a known system ID. */
    static Result scanStructured(const QString& romsBaseDir,
                                 const ManifestLoader& loader,
                                 Database& db);

    /** Parse an M3U file and return canonical absolute paths of referenced files.
     *  Skips comments (#), empty lines, and non-existent entries. */
    static QSet<QString> parseM3u(const QString& m3uPath);

    /** Detect multi-disc ROM groups in a directory and auto-generate M3U playlists.
     *  Only generates for groups of 2+ files with matching disc-indicator patterns,
     *  same extension, same directory, and no existing M3U. Returns number of M3Us created. */
    static int autoGenerateM3u(const QString& directory);
};
