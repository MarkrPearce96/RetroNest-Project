#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "core/iso9660_reader.h"

class TestIso9660Reader : public QObject {
    Q_OBJECT

private slots:

    // ── parseSystemCnfSerial ─────────────────────────────────────────────────

    void testParsePs2Serial() {
        QByteArray content = "BOOT2 = cdrom0:\\SLUS_200.62;1\r\nVER = 1.00\r\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString("SLUS_200.62"));
    }

    void testParsePs1Serial() {
        QByteArray content = "BOOT = cdrom:\\SCUS_941.83;1\r\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString("SCUS_941.83"));
    }

    void testParseForwardSlashVariant() {
        QByteArray content = "BOOT2 = cdrom0:/SLES_123.45;1\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString("SLES_123.45"));
    }

    void testParseNoVersionSuffix() {
        // Some early PS1 games omit the ;1 version suffix
        QByteArray content = "BOOT = cdrom:\\SCES_000.63\r\nTCB = 4\r\nEVENT = 10\r\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString("SCES_000.63"));
    }

    void testParseEmptyContent() {
        QCOMPARE(Iso9660::parseSystemCnfSerial(QByteArray()), QString());
    }

    void testParseGarbageContent() {
        QByteArray content = "this is not a valid SYSTEM.CNF file at all\n";
        QCOMPARE(Iso9660::parseSystemCnfSerial(content), QString());
    }

    // ── resolveToDataFile ────────────────────────────────────────────────────

    void testResolveCueFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString cuePath = dir.filePath("game.cue");
        QString binPath = dir.filePath("game.bin");

        // Create a minimal .cue file
        {
            QFile cue(cuePath);
            QVERIFY(cue.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&cue);
            out << "FILE \"game.bin\" BINARY\n";
            out << "  TRACK 01 MODE2/2352\n";
            out << "    INDEX 01 00:00:00\n";
        }

        // The .bin doesn't need to exist for resolveToDataFile; it just resolves the path.
        QString resolved = Iso9660::resolveToDataFile(cuePath);
        QCOMPARE(resolved, binPath);
    }

    void testResolveM3uFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString disc1Path = dir.filePath("game_disc1.bin");
        QString m3uPath   = dir.filePath("game.m3u");

        // Create the referenced .bin so resolveToDataFile can confirm it exists
        // (the function doesn't actually require the file, but the recursion will
        //  hit the passthrough branch for .bin and return the path).
        {
            QFile f(disc1Path);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }

        // Create the .m3u
        {
            QFile m3u(m3uPath);
            QVERIFY(m3u.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&m3u);
            out << "# Multi-disc game\n";
            out << "game_disc1.bin\n";
            out << "game_disc2.bin\n";
        }

        QString resolved = Iso9660::resolveToDataFile(m3uPath);
        QCOMPARE(resolved, disc1Path);
    }

    void testResolveIsoPassthrough() {
        QString isoPath = "/some/path/game.iso";
        QCOMPARE(Iso9660::resolveToDataFile(isoPath), isoPath);
    }

    void testResolveBinPassthrough() {
        QString binPath = "/some/path/game.bin";
        QCOMPARE(Iso9660::resolveToDataFile(binPath), binPath);
    }

    // ── detectSectorSize ─────────────────────────────────────────────────────

    void testDetectCookedSector() {
        // Cooked data — no sync pattern; first byte is arbitrary.
        QByteArray data(2352, '\x00');
        data[0] = 0x01; // break the sync pattern
        QCOMPARE(Iso9660::detectSectorSize(data), 2048);
    }

    void testDetectRawSector() {
        // Raw sector — starts with CD-ROM sync pattern:
        // 00 FF FF FF FF FF FF FF FF FF FF 00 ...
        QByteArray data(2352, '\xFF');
        data[0]  = static_cast<char>(0x00);
        data[11] = static_cast<char>(0x00);
        QCOMPARE(Iso9660::detectSectorSize(data), 2352);
    }

    void testDetectSectorSizeShortBuffer() {
        // Buffer shorter than 12 bytes — should default to cooked.
        QByteArray data(8, '\xFF');
        data[0] = static_cast<char>(0x00);
        QCOMPARE(Iso9660::detectSectorSize(data), 2048);
    }
};

QTEST_MAIN(TestIso9660Reader)
#include "test_iso9660_reader.moc"
