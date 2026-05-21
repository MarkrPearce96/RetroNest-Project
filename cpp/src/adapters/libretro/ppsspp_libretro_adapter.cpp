#include "ppsspp_libretro_adapter.h"
#include "core/iso9660_reader.h"
#include "core/sfo_parser.h"
#include <QDebug>

QVector<ControllerTypeDef> PpssppLibretroAdapter::controllerTypes() const {
    return {
        {"Standard", "PSP Controller",
         ":/AppUI/qml/AppUI/images/controllers/PSP.svg"},
    };
}

QVector<PathDef> PpssppLibretroAdapter::pathsDefs() const {
    return {
        { "Saves",       "libretro", "Saves",      "saves",      PathBase::EmulatorData },
        { "Save States", "libretro", "SaveStates", "savestates", PathBase::EmulatorData },
    };
}

QString PpssppLibretroAdapter::extractSerial(const QString& romPath) const {
    QByteArray sfoData = Iso9660::readFile(romPath, "PSP_GAME/PARAM.SFO");
    if (sfoData.isEmpty()) {
        qWarning() << "[PPSSPP-libretro] Failed to read PSP_GAME/PARAM.SFO from:" << romPath;
        return {};
    }
    return SfoParser::extractDiscId(sfoData);
}
