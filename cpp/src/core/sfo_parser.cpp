#include "sfo_parser.h"
#include <QDataStream>
#include <QDebug>

namespace SfoParser {

struct SfoHeader {
    quint32 magic;
    quint32 version;
    quint32 keyTableStart;
    quint32 dataTableStart;
    quint32 indexCount;
};

struct SfoIndexEntry {
    quint16 keyOffset;
    quint16 dataFormat;
    quint32 dataLen;
    quint32 dataMaxLen;
    quint32 dataOffset;
};

static const quint32 SFO_MAGIC = 0x46535000;  // "\0PSF" read as little-endian u32
static const quint16 SFO_FORMAT_UTF8 = 0x0204;

QString extractStringValue(const QByteArray& sfoData, const QString& key) {
    if (sfoData.size() < 20)
        return {};

    QDataStream ds(sfoData);
    ds.setByteOrder(QDataStream::LittleEndian);

    SfoHeader header;
    ds >> header.magic >> header.version >> header.keyTableStart
       >> header.dataTableStart >> header.indexCount;

    if (header.magic != SFO_MAGIC)
        return {};

    const auto totalSize = static_cast<quint32>(sfoData.size());
    if (header.keyTableStart > totalSize || header.dataTableStart > totalSize)
        return {};

    const QByteArray keyBytes = key.toUtf8();

    for (quint32 i = 0; i < header.indexCount; ++i) {
        SfoIndexEntry entry;
        ds >> entry.keyOffset >> entry.dataFormat >> entry.dataLen
           >> entry.dataMaxLen >> entry.dataOffset;

        if (ds.status() != QDataStream::Ok)
            return {};

        const quint32 keyPos = header.keyTableStart + entry.keyOffset;
        if (keyPos >= totalSize)
            continue;

        const char* keyStart = sfoData.constData() + keyPos;
        const int maxKeyLen = totalSize - keyPos;
        QByteArray entryKey(keyStart, qstrnlen(keyStart, maxKeyLen));

        if (entryKey != keyBytes)
            continue;

        if (entry.dataFormat != SFO_FORMAT_UTF8)
            return {};

        const quint32 dataPos = header.dataTableStart + entry.dataOffset;
        if (dataPos >= totalSize || dataPos + entry.dataLen > totalSize)
            return {};

        return QString::fromUtf8(sfoData.constData() + dataPos, entry.dataLen - 1);
    }

    return {};
}

}
