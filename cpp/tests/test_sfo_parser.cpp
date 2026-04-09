#include <QtTest>
#include "core/sfo_parser.h"

class TestSfoParser : public QObject {
    Q_OBJECT

private:
    static QByteArray buildSfo(const QByteArray& key, const QByteArray& value) {
        const quint32 magic = 0x46535000;  // "\0PSF" as little-endian u32
        const quint32 version = 0x00000101;
        const quint32 indexCount = 1;
        const quint32 headerSize = 20;
        const quint32 indexSize = 16;

        const quint32 keyTableStart = headerSize + (indexSize * indexCount);
        const quint32 keyLen = key.size() + 1;
        const quint32 dataTableStart = keyTableStart + ((keyLen + 3) & ~3u);

        const quint16 keyOffset = 0;
        const quint16 dataFormat = 0x0204;
        const quint32 dataLen = value.size() + 1;
        const quint32 dataMaxLen = (dataLen + 3) & ~3u;
        const quint32 dataOffset = 0;

        QByteArray sfo;
        QDataStream ds(&sfo, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::LittleEndian);

        ds << magic << version << keyTableStart << dataTableStart << indexCount;
        ds << keyOffset << dataFormat << dataLen << dataMaxLen << dataOffset;

        sfo.append(key);
        sfo.append('\0');
        while (static_cast<quint32>(sfo.size()) < dataTableStart)
            sfo.append('\0');

        sfo.append(value);
        sfo.append('\0');
        while (sfo.size() % 4 != 0)
            sfo.append('\0');

        return sfo;
    }

private slots:
    void testExtractDiscId() {
        QByteArray sfo = buildSfo("DISC_ID", "ULES00151");
        QCOMPARE(SfoParser::extractDiscId(sfo), QString("ULES00151"));
    }

    void testExtractTitle() {
        QByteArray sfo = buildSfo("TITLE", "LocoRoco");
        QCOMPARE(SfoParser::extractStringValue(sfo, "TITLE"), QString("LocoRoco"));
    }

    void testKeyNotFound() {
        QByteArray sfo = buildSfo("DISC_ID", "ULES00151");
        QCOMPARE(SfoParser::extractStringValue(sfo, "MISSING_KEY"), QString());
    }

    void testEmptyData() {
        QCOMPARE(SfoParser::extractDiscId(QByteArray()), QString());
    }

    void testTruncatedHeader() {
        QByteArray truncated(10, '\0');
        QCOMPARE(SfoParser::extractDiscId(truncated), QString());
    }

    void testBadMagic() {
        QByteArray sfo = buildSfo("DISC_ID", "ULES00151");
        sfo[0] = 'X';
        QCOMPARE(SfoParser::extractDiscId(sfo), QString());
    }
};

QTEST_GUILESS_MAIN(TestSfoParser)
#include "test_sfo_parser.moc"
