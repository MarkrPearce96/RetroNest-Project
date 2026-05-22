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
        // D-Pad — cross at left-middle. Up/Down share x=285, Left/Right share y=475.
        { BindingDef::Button, "D-Pad Up",    "D-Pad",   "Pad1", "Up",    "SDL-0/DPadUp",        "DPad",         272, 335, 50 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",   "Pad1", "Down",  "SDL-0/DPadDown",      "DPad",         272, 595, 50 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",   "Pad1", "Left",  "SDL-0/DPadLeft",      "DPad",         145, 465, 50 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",   "Pad1", "Right", "SDL-0/DPadRight",     "DPad",         405, 465, 50 },
        // Face buttons — cluster center ~(2050, 620). Shifted down +60 vs.
        // first cut because the diamond sits below the LCD vertical centre.
        { BindingDef::Button, "Cross",       "Buttons", "Pad1", "B",     "SDL-0/FaceSouth",     "FaceButtons", 2110, 600, 45 },
        { BindingDef::Button, "Circle",      "Buttons", "Pad1", "A",     "SDL-0/FaceEast",      "FaceButtons", 2247, 462, 45 },
        { BindingDef::Button, "Square",      "Buttons", "Pad1", "Y",     "SDL-0/FaceWest",      "FaceButtons", 1973, 462, 45 },
        { BindingDef::Button, "Triangle",    "Buttons", "Pad1", "X",     "SDL-0/FaceNorth",     "FaceButtons", 2110, 320, 45 },
        // Shoulders — the white shapes at the top-left and top-right
        // corners of the body ARE the L and R buttons on a PSP-1000.
        { BindingDef::Button, "L",           "Shoulders", "Pad1", "L",   "SDL-0/LeftShoulder",  "Shoulders",    250, 75, 50 },
        { BindingDef::Button, "R",           "Shoulders", "Pad1", "R",   "SDL-0/RightShoulder", "Shoulders",   2120, 75, 50 },
        // System — Start sits at far right of the bottom icon row, Select
        // is one slot to the left. First-cut x was on the Display icon.
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "SDL-0/Start",      "System",      1850, 950, 30 },
        { BindingDef::Button, "Select",      "System",    "Pad1", "Select", "SDL-0/Back",       "System",      1700, 950, 30 },
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
