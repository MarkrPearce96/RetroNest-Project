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

QVector<BindingDef> PpssppLibretroAdapter::controllerBindingDefsForType(const QString&) const {
    // PSP-1000 horizontal layout. Spotlight coords target PSP.svg's
    // viewBox (2367 x 1014). PlayStation face-button conventions:
    //   Cross  (south) -> RetroPad B
    //   Circle (east)  -> RetroPad A
    //   Square (west)  -> RetroPad Y
    //   Triangle (north) -> RetroPad X
    // The analog nub feeds RetroPad axes at the runtime layer and is
    // not part of this digital-binding surface.
    return {
        // D-Pad — cross at left-middle
        { BindingDef::Button, "D-Pad Up",    "D-Pad",   "Pad1", "Up",    "SDL-0/DPadUp",        "DPad",         325, 410, 55 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",   "Pad1", "Down",  "SDL-0/DPadDown",      "DPad",         325, 610, 55 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",   "Pad1", "Left",  "SDL-0/DPadLeft",      "DPad",         225, 510, 55 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",   "Pad1", "Right", "SDL-0/DPadRight",     "DPad",         425, 510, 55 },
        // Face buttons — cluster at right-middle
        { BindingDef::Button, "Cross",       "Buttons", "Pad1", "B",     "SDL-0/FaceSouth",     "FaceButtons", 1900, 610, 50 },
        { BindingDef::Button, "Circle",      "Buttons", "Pad1", "A",     "SDL-0/FaceEast",      "FaceButtons", 2000, 510, 50 },
        { BindingDef::Button, "Square",      "Buttons", "Pad1", "Y",     "SDL-0/FaceWest",      "FaceButtons", 1800, 510, 50 },
        { BindingDef::Button, "Triangle",    "Buttons", "Pad1", "X",     "SDL-0/FaceNorth",     "FaceButtons", 1900, 410, 50 },
        // Shoulders — top corners
        { BindingDef::Button, "L",           "Shoulders", "Pad1", "L",   "SDL-0/LeftShoulder",  "Shoulders",    130, 150, 60 },
        { BindingDef::Button, "R",           "Shoulders", "Pad1", "R",   "SDL-0/RightShoulder", "Shoulders",   2240, 150, 60 },
        // System — Start + Select centered low
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "SDL-0/Start",      "System",      1450, 870, 35 },
        { BindingDef::Button, "Select",      "System",    "Pad1", "Select", "SDL-0/Back",       "System",      1330, 870, 35 },
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
