#pragma once

#include <QByteArray>
#include <QString>

namespace SfoParser {
    QString extractStringValue(const QByteArray& sfoData, const QString& key);

    inline QString extractDiscId(const QByteArray& sfoData) {
        return extractStringValue(sfoData, "DISC_ID");
    }
}
