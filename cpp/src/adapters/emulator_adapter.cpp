#include "emulator_adapter.h"
#include "core/iso9660_reader.h"

#include <QDebug>

QString EmulatorAdapter::extractSerial(const QString& romPath) const {
    QByteArray content = Iso9660::readFile(romPath, "SYSTEM.CNF");
    if (content.isEmpty()) {
        qWarning() << "[EmulatorAdapter] Failed to read SYSTEM.CNF from:" << romPath;
        return {};
    }
    QString serial = Iso9660::parseSystemCnfSerial(content);
    if (serial.isEmpty()) {
        qWarning() << "[EmulatorAdapter] No serial found in SYSTEM.CNF for:" << romPath;
    }
    return serial;
}
