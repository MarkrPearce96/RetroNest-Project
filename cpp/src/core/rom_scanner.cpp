#include "rom_scanner.h"
#include "paths.h"
#include "adapters/adapter_registry.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QHash>
#include <QSet>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

static QString titleFromFilename(const QString& filename) {
    // Strip extension, replace common separators with spaces
    QString name = QFileInfo(filename).completeBaseName();
    name.replace('_', ' ');
    name.replace('.', ' ');
    // Trim region tags like (USA), [NTSC], etc. — keep them but clean up
    return name.trimmed();
}

QSet<QString> RomScanner::parseM3u(const QString& m3uPath) {
    QSet<QString> paths;
    QFile file(m3uPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[Scanner] Cannot open M3U file:" << m3uPath;
        return paths;
    }

    const QDir m3uDir = QFileInfo(m3uPath).absoluteDir();
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        // Resolve relative paths against the M3U file's directory
        QString resolved;
        if (QDir::isAbsolutePath(line))
            resolved = line;
        else
            resolved = m3uDir.filePath(line);

        QString canonical = QFileInfo(resolved).canonicalFilePath();
        if (!canonical.isEmpty())
            paths.insert(canonical);
    }

    return paths;
}

int RomScanner::autoGenerateM3u(const QString& directory) {
    // Regex matches: (Disc N), (Disk N), (CD N), (Disc N of M) — case-insensitive
    static const QRegularExpression discPattern(
        R"(\((?:Disc|Disk|CD)\s*(\d+)(?:\s*of\s*\d+)?\))",
        QRegularExpression::CaseInsensitiveOption);

    // Group key: baseName + extension + directory
    struct DiscEntry {
        int discNumber;
        QString fileName;
    };
    // key = "dirPath|baseName|ext"
    QHash<QString, QVector<DiscEntry>> groups;

    QDirIterator it(directory, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();
        const QString fileName = fi.fileName();

        auto match = discPattern.match(fileName);
        if (!match.hasMatch()) continue;

        int discNum = match.captured(1).toInt();

        // Strip the disc indicator to get the base name
        QString baseName = fileName.left(match.capturedStart())
                         + fileName.mid(match.capturedEnd());
        // Remove extension to get the base title
        baseName = QFileInfo(baseName).completeBaseName().trimmed();
        const QString ext = fi.suffix().toLower();
        const QString dir = fi.absolutePath();

        QString key = dir + "|" + baseName + "|" + ext;
        groups[key].append({discNum, fileName});
    }

    int created = 0;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        const auto& entries = it.value();
        if (entries.size() < 2) continue;

        // Parse the key to get dir and baseName
        const QStringList parts = it.key().split('|');
        const QString dir = parts[0];
        const QString baseName = parts[1];

        // Check if M3U already exists
        const QString m3uPath = dir + "/" + baseName + ".m3u";
        if (QFileInfo::exists(m3uPath)) continue;

        // Sort by disc number
        QVector<DiscEntry> sorted = entries;
        std::sort(sorted.begin(), sorted.end(),
                  [](const DiscEntry& a, const DiscEntry& b) { return a.discNumber < b.discNumber; });

        // Write M3U
        QFile m3uFile(m3uPath);
        if (!m3uFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[Scanner] Cannot create M3U file:" << m3uPath;
            continue;
        }
        QTextStream out(&m3uFile);
        for (const auto& entry : sorted)
            out << entry.fileName << "\n";

        created++;
        qInfo() << "[Scanner] Auto-generated:" << baseName + ".m3u"
                << "(" << sorted.size() << "discs)";
    }

    return created;
}

RomScanner::Result RomScanner::scan(const QString& directory,
                                     const ManifestLoader& loader,
                                     Database& db,
                                     const QString& systemFilter) {
    Result result;

    QDir dir(directory);
    if (!dir.exists()) {
        qWarning() << "[Scanner] Directory does not exist:" << directory;
        return result;
    }

    // Build extension → (system, emulator_id) lookup from manifests.
    // When systemFilter is set, only that system's emulator participates,
    // so shared extensions (bin, iso, chd) resolve unambiguously.
    // When unfiltered, conflicts are logged and first-loaded wins.
    struct SystemInfo { QString system; QString emuId; };
    QHash<QString, SystemInfo> extMap;
    QStringList conflicts;

    for (const auto& emu : loader.allEmulators()) {
        for (const auto& system : emu.systems) {
            if (!systemFilter.isEmpty() && system != systemFilter) continue;
            for (const auto& ext : emu.rom_extensions) {
                const QString lext = ext.toLower();
                if (extMap.contains(lext)) {
                    if (extMap[lext].emuId != emu.id) {
                        conflicts << QString(".%1 (%2 vs %3)").arg(lext, extMap[lext].emuId, emu.id);
                    }
                } else {
                    extMap.insert(lext, {system, emu.id});
                }
            }
        }
    }

    if (!conflicts.isEmpty() && systemFilter.isEmpty()) {
        qWarning() << "[Scanner] Ambiguous file extensions — place ROMs in system subfolders"
                    << "(e.g. roms/psx/, roms/ps2/) for correct detection:" << conflicts.join(", ");
    }

    // Directories to exclude from scanning (e.g. BIOS folder)
    const QString biosDir = QDir(Paths::biosDir()).canonicalPath();

    // Auto-detect multi-disc groups and generate M3U playlists
    autoGenerateM3u(directory);

    // Pass 1: collect files referenced by M3U playlists (suppression set)
    QSet<QString> suppressedPaths;
    {
        QDirIterator m3uIt(directory, {"*.m3u"}, QDir::Files, QDirIterator::Subdirectories);
        while (m3uIt.hasNext()) {
            m3uIt.next();
            if (!biosDir.isEmpty() && m3uIt.fileInfo().canonicalPath().startsWith(biosDir))
                continue;
            suppressedPaths.unite(parseM3u(m3uIt.filePath()));
        }
    }

    // Scan directory recursively
    QDirIterator dirIt(directory, QDir::Files, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        const QFileInfo fi = dirIt.fileInfo();

        // Skip files inside the BIOS directory
        if (!biosDir.isEmpty() && fi.canonicalPath().startsWith(biosDir))
            continue;

        // Skip files referenced by an M3U playlist
        if (suppressedPaths.contains(fi.canonicalFilePath()))
            continue;

        const QString ext = fi.suffix().toLower();
        auto it = extMap.find(ext);
        if (it == extMap.end()) continue;

        const QString romPath = fi.absoluteFilePath();

        if (db.gameExistsByPath(romPath)) {
            result.skipped++;
            continue;
        }

        GameRecord game;
        game.title = titleFromFilename(fi.fileName());
        game.rom_path = romPath;
        game.system = it->system;
        game.emulator_id = it->emuId;

        // Set disc count for M3U files
        if (ext == "m3u") {
            game.disc_count = static_cast<int>(parseM3u(romPath).size());
        }

        int addResult = db.addGame(game);
        if (addResult > 0) {
            result.added++;
            qInfo() << "[Scanner] Added:" << game.title << "[" << game.system << "]";

            // Extract serial from ROM and store in DB
            auto* adapter = AdapterRegistry::instance().adapterFor(game.emulator_id);
            if (adapter) {
                QString serial = adapter->extractSerial(romPath);
                if (!serial.isEmpty()) {
                    db.updateSerial(addResult, serial);
                    qInfo() << "[Scanner] Serial:" << serial << "for" << game.title;
                }
            }
        } else if (addResult == 0) {
            result.skipped++;
        } else {
            qWarning() << "[Scanner] Failed to add:" << game.title;
        }
    }

    // Remove stale DB entries for files that are now suppressed by M3U playlists
    if (!suppressedPaths.isEmpty()) {
        for (const QString& suppressed : suppressedPaths) {
            if (db.removeGameByPath(suppressed)) {
                qInfo() << "[Scanner] Removed stale disc entry:" << suppressed;
            }
        }
    }

    qInfo() << "[Scanner] Scan complete:" << result.added << "added," << result.skipped << "skipped";
    return result;
}

RomScanner::Result RomScanner::scanStructured(const QString& romsBaseDir,
                                               const ManifestLoader& loader,
                                               Database& db) {
    Result total;

    QDir baseDir(romsBaseDir);
    if (!baseDir.exists()) {
        qWarning() << "[Scanner] ROM base directory does not exist:" << romsBaseDir;
        return total;
    }

    // Build set of known system IDs from manifests
    QSet<QString> knownSystems;
    for (const auto& emu : loader.allEmulators()) {
        for (const auto& sys : emu.systems)
            knownSystems.insert(sys);
    }

    // Scan each subdirectory whose name matches a system ID
    for (const auto& entry : baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!knownSystems.contains(entry)) {
            qInfo() << "[Scanner] Skipping unknown system folder:" << entry;
            continue;
        }
        QString subdir = romsBaseDir + "/" + entry;
        auto result = scan(subdir, loader, db, entry);
        total.added += result.added;
        total.skipped += result.skipped;
    }

    qInfo() << "[Scanner] Structured scan complete:" << total.added << "added," << total.skipped << "skipped";
    return total;
}
