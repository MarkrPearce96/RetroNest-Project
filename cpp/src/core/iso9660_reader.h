#pragma once

#include <QByteArray>
#include <QString>

namespace Iso9660 {
    QByteArray readFile(const QString& imagePath, const QString& filename);
    QString parseSystemCnfSerial(const QByteArray& content);
    QString resolveToDataFile(const QString& path);
    int detectSectorSize(const QByteArray& firstBytes);
}
