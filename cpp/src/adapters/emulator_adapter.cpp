#include "emulator_adapter.h"
#include "core/iso9660_reader.h"

#include <QDebug>

QString EmulatorAdapter::extractSerial(const QString& romPath) const {
    QByteArray content = Iso9660::readFile(romPath, "SYSTEM.CNF");
    if (content.isEmpty()) {
        // Not an error: homebrew discs (e.g. Magic Castle) legitimately ship
        // no SYSTEM.CNF and boot the BIOS-default PSX.EXE, so there's no
        // license serial to read. The caller falls back to the filename as
        // the id, which is correct for these discs.
        qInfo() << "[EmulatorAdapter] No SYSTEM.CNF in" << romPath
                << "— using filename as id";
        return {};
    }
    QString serial = Iso9660::parseSystemCnfSerial(content);
    if (serial.isEmpty()) {
        qWarning() << "[EmulatorAdapter] No serial found in SYSTEM.CNF for:" << romPath;
    }
    return serial;
}
