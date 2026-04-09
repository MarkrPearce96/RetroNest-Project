#include "iso9660_reader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QtDebug>

#include <libchdr/chd.h>

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr int COOKED_SECTOR = 2048;
static constexpr int RAW_SECTOR    = 2352;
static constexpr int MODE1_HEADER  = 16;   // sync (12) + header (4)
static constexpr int MODE2_HEADER  = 24;   // sync (12) + header (4) + sub-header (8)
static constexpr int PVD_SECTOR    = 16;

// CD-ROM sync pattern: 00 FF FF FF FF FF FF FF FF FF FF 00
static const quint8 CD_SYNC[12] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

// ── SectorReader interface ─────────────────────────────────────────────────────

class SectorReader {
public:
    virtual ~SectorReader() = default;
    // Read 2048 bytes of user data from the given logical sector index.
    virtual bool readSector(quint32 sectorIndex, quint8* outData) = 0;
};

// ── FileSectorReader (.iso / .bin) ─────────────────────────────────────────────

class FileSectorReader : public SectorReader {
public:
    FileSectorReader(const QString& path, int sectorSize)
        : m_file(path), m_sectorSize(sectorSize)
    {
        (void)m_file.open(QIODevice::ReadOnly);
    }

    bool readSector(quint32 sectorIndex, quint8* outData) override {
        if (!m_file.isOpen())
            return false;

        qint64 sectorStart = static_cast<qint64>(sectorIndex) * m_sectorSize;

        if (m_sectorSize == RAW_SECTOR) {
            // Read mode byte at offset 15 within the sector to determine header size.
            // Mode 1: user data at offset 16. Mode 2 Form 1: user data at offset 24.
            if (!m_file.seek(sectorStart + 15))
                return false;
            char modeByte = 0;
            if (m_file.read(&modeByte, 1) != 1)
                return false;
            int headerSize = (modeByte == 2) ? MODE2_HEADER : MODE1_HEADER;

            if (!m_file.seek(sectorStart + headerSize))
                return false;
        } else {
            if (!m_file.seek(sectorStart))
                return false;
        }

        qint64 read = m_file.read(reinterpret_cast<char*>(outData), COOKED_SECTOR);
        return read == COOKED_SECTOR;
    }

private:
    QFile m_file;
    int   m_sectorSize;
};

// ── ChdSectorReader (.chd) ─────────────────────────────────────────────────────

class ChdSectorReader : public SectorReader {
public:
    explicit ChdSectorReader(const QString& path)
    {
        chd_error err = chd_open(path.toUtf8().constData(), CHD_OPEN_READ, nullptr, &m_chd);
        if (err != CHDERR_NONE) {
            qWarning() << "ChdSectorReader: chd_open failed for" << path << "error:" << err;
            m_chd = nullptr;
            return;
        }

        const chd_header* hdr = chd_get_header(m_chd);
        m_hunkBytes  = hdr->hunkbytes;
        m_unitBytes  = hdr->unitbytes;
        m_unitsPerHunk = (m_unitBytes > 0) ? (m_hunkBytes / m_unitBytes) : 1;

        m_hunkBuf.resize(m_hunkBytes);
        m_cachedHunk = static_cast<quint32>(-1);
    }

    ~ChdSectorReader() override {
        if (m_chd)
            chd_close(m_chd);
    }

    bool isOpen() const { return m_chd != nullptr; }

    bool readSector(quint32 sectorIndex, quint8* outData) override {
        if (!m_chd)
            return false;

        quint32 hunkIndex = sectorIndex / m_unitsPerHunk;
        quint32 unitWithinHunk = sectorIndex % m_unitsPerHunk;

        if (hunkIndex != m_cachedHunk) {
            chd_error err = chd_read(m_chd, hunkIndex,
                                     reinterpret_cast<void*>(m_hunkBuf.data()));
            if (err != CHDERR_NONE) {
                qWarning() << "ChdSectorReader: chd_read failed for hunk" << hunkIndex;
                return false;
            }
            m_cachedHunk = hunkIndex;
        }

        // Each unit is m_unitBytes wide; detect mode from byte 15 within the sector.
        const quint8* unitStart = reinterpret_cast<const quint8*>(m_hunkBuf.constData())
                                  + unitWithinHunk * m_unitBytes;

        // Check mode byte at offset 15: Mode 1 → header 16, Mode 2 → header 24.
        int headerSize = (unitStart[15] == 2) ? MODE2_HEADER : MODE1_HEADER;
        const quint8* userData = unitStart + headerSize;
        memcpy(outData, userData, COOKED_SECTOR);
        return true;
    }

private:
    chd_file*          m_chd        = nullptr;
    quint32            m_hunkBytes  = 0;
    quint32            m_unitBytes  = 0;
    quint32            m_unitsPerHunk = 1;
    QByteArray         m_hunkBuf;
    quint32            m_cachedHunk;
};

// ── ISO9660 parsing helpers ────────────────────────────────────────────────────

// Read a little-endian 16-bit value.
static quint16 le16(const quint8* p) {
    return static_cast<quint16>(p[0]) | (static_cast<quint16>(p[1]) << 8);
}

// Read a little-endian 32-bit value.
static quint32 le32(const quint8* p) {
    return static_cast<quint32>(p[0])
         | (static_cast<quint32>(p[1]) << 8)
         | (static_cast<quint32>(p[2]) << 16)
         | (static_cast<quint32>(p[3]) << 24);
}

struct DirEntry {
    QString  name;
    quint32  lba;
    quint32  size;
    bool     isDir;
};

// Parse a single directory entry from raw bytes.
// Returns the length of the entry record (0 on failure).
static int parseDirEntry(const quint8* p, int available, DirEntry& out) {
    if (available < 1) return 0;
    int recLen = p[0];
    if (recLen == 0 || recLen > available || recLen < 33)
        return 0;

    out.lba    = le32(p + 2);
    out.size   = le32(p + 10);
    out.isDir  = (p[25] & 0x02) != 0;

    int nameLen = p[32];
    if (nameLen == 0 || nameLen > recLen - 33)
        return recLen; // dot / dotdot entries — leave name empty

    // Remove version suffix (;1)
    QByteArray raw(reinterpret_cast<const char*>(p + 33), nameLen);
    int semi = raw.indexOf(';');
    if (semi >= 0)
        raw = raw.left(semi);
    out.name = QString::fromLatin1(raw).toUpper();

    return recLen;
}

// Walk a directory sector chain looking for a file named `target` (upper-case).
// Returns true and fills `out` when found.
static bool findInDirectory(SectorReader& reader,
                            quint32 dirLba, quint32 dirSize,
                            const QString& target, DirEntry& out)
{
    quint8 sectorBuf[COOKED_SECTOR];
    quint32 bytesLeft = dirSize;
    quint32 lba = dirLba;

    while (bytesLeft > 0) {
        if (!reader.readSector(lba, sectorBuf))
            return false;

        quint32 sectorBytes = qMin(bytesLeft, static_cast<quint32>(COOKED_SECTOR));
        int offset = 0;

        while (offset < static_cast<int>(sectorBytes)) {
            DirEntry entry;
            int len = parseDirEntry(sectorBuf + offset,
                                    static_cast<int>(sectorBytes) - offset, entry);
            if (len == 0) break;

            if (!entry.name.isEmpty() && entry.name == target) {
                out = entry;
                return true;
            }
            offset += len;
        }

        ++lba;
        bytesLeft = (bytesLeft > COOKED_SECTOR) ? bytesLeft - COOKED_SECTOR : 0;
    }
    return false;
}

// ── Public API ─────────────────────────────────────────────────────────────────

namespace Iso9660 {

int detectSectorSize(const QByteArray& firstBytes) {
    // Look for CD-ROM sync pattern in the first ~2352 bytes.
    if (firstBytes.size() >= 12 &&
        memcmp(firstBytes.constData(), CD_SYNC, 12) == 0)
    {
        return RAW_SECTOR;
    }
    return COOKED_SECTOR;
}

QString resolveToDataFile(const QString& path) {
    QString lower = path.toLower();

    if (lower.endsWith(".cue")) {
        // Parse FILE line to get the .bin path.
        QFile cue(path);
        if (!cue.open(QIODevice::ReadOnly | QIODevice::Text))
            return path;

        QTextStream in(&cue);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("FILE", Qt::CaseInsensitive)) {
                // FILE "name.bin" BINARY
                int first = line.indexOf('"');
                int last  = line.lastIndexOf('"');
                if (first >= 0 && last > first) {
                    QString binName = line.mid(first + 1, last - first - 1);
                    QFileInfo info(QDir(QFileInfo(path).absolutePath()), binName);
                    return info.absoluteFilePath();
                }
            }
        }
        return path; // fallback
    }

    if (lower.endsWith(".m3u")) {
        // Read first non-empty, non-comment entry and recurse.
        QFile m3u(path);
        if (!m3u.open(QIODevice::ReadOnly | QIODevice::Text))
            return path;

        QTextStream in(&m3u);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            // Entry may be absolute or relative to the .m3u directory.
            QFileInfo info(line);
            if (!info.isAbsolute())
                info = QFileInfo(QDir(QFileInfo(path).absolutePath()), line);
            return resolveToDataFile(info.absoluteFilePath());
        }
        return path; // empty M3U
    }

    return path; // .iso, .bin, .chd — return as-is
}

QByteArray readFile(const QString& imagePath, const QString& filename) {
    QString dataPath = resolveToDataFile(imagePath);
    QString lower    = dataPath.toLower();

    // ── Create sector reader ──────────────────────────────────────────────────
    std::unique_ptr<SectorReader> reader;

    if (lower.endsWith(".chd")) {
        auto chd = std::make_unique<ChdSectorReader>(dataPath);
        if (!chd->isOpen())
            return {};
        reader = std::move(chd);
    } else {
        // .iso or .bin — detect sector size from the first bytes.
        QFile f(dataPath);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        QByteArray header = f.read(RAW_SECTOR);
        f.close();

        int sectorSize = detectSectorSize(header);
        reader = std::make_unique<FileSectorReader>(dataPath, sectorSize);
    }

    // ── Read PVD (sector 16) ──────────────────────────────────────────────────
    quint8 pvd[COOKED_SECTOR];
    if (!reader->readSector(PVD_SECTOR, pvd))
        return {};

    // Verify "CD001" signature at offset 1.
    if (pvd[1] != 'C' || pvd[2] != 'D' || pvd[3] != '0' ||
        pvd[4] != '0' || pvd[5] != '1')
    {
        qWarning() << "iso9660_reader: PVD signature mismatch in" << dataPath;
        return {};
    }

    // Root directory entry is at offset 156 in the PVD (34 bytes).
    quint32 rootLba  = le32(pvd + 156 + 2);
    quint32 rootSize = le32(pvd + 156 + 10);

    // ── Walk path components ──────────────────────────────────────────────────
    // Strip leading slashes and normalise to upper-case parts.
    QString norm = filename;
    while (norm.startsWith('/') || norm.startsWith('\\'))
        norm = norm.mid(1);
    QStringList parts = norm.toUpper().split(QRegularExpression("[/\\\\]"),
                                              Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};

    quint32 curLba  = rootLba;
    quint32 curSize = rootSize;

    for (int i = 0; i < parts.size() - 1; ++i) {
        DirEntry entry;
        if (!findInDirectory(*reader, curLba, curSize, parts[i], entry) || !entry.isDir)
            return {};
        curLba  = entry.lba;
        curSize = entry.size;
    }

    DirEntry fileEntry;
    if (!findInDirectory(*reader, curLba, curSize, parts.last(), fileEntry) || fileEntry.isDir)
        return {};

    // ── Read file data ────────────────────────────────────────────────────────
    QByteArray result;
    result.reserve(static_cast<qsizetype>(fileEntry.size));

    quint8 sectorBuf[COOKED_SECTOR];
    quint32 bytesLeft = fileEntry.size;
    quint32 lba = fileEntry.lba;

    while (bytesLeft > 0) {
        if (!reader->readSector(lba, sectorBuf))
            break;
        quint32 chunk = qMin(bytesLeft, static_cast<quint32>(COOKED_SECTOR));
        result.append(reinterpret_cast<const char*>(sectorBuf),
                      static_cast<qsizetype>(chunk));
        bytesLeft -= chunk;
        ++lba;
    }

    return result;
}

QString parseSystemCnfSerial(const QByteArray& content) {
    static const QRegularExpression re(
        R"(BOOT2?\s*=\s*cdrom0?:?[/\\]?([^;\s]+?)(?:;1)?\s*$)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption
    );

    QString text = QString::fromLatin1(content);
    QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return {};

    QString captured = m.captured(1);
    // Extract filename after last slash/backslash.
    int slash = captured.lastIndexOf('/');
    int bslash = captured.lastIndexOf('\\');
    int sep = qMax(slash, bslash);
    if (sep >= 0)
        captured = captured.mid(sep + 1);

    return captured;
}

} // namespace Iso9660
