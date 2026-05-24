#include "dolphin_libretro_adapter.h"

int DolphinLibretroAdapter::raConsoleId(const QString& systemId) const {
    if (systemId == "gc")
        return 16;
    if (systemId == "wii")
        return 19;
    return 0;
}

namespace {

// GameCube controller. GameCube.svg viewBox is 0 0 1799 1368; spotlight coords
// reused from the deleted standalone adapter's calibration.
//
// Keys are RetroPad slots (retroPadSlotFromKey). RetroNest seeds default
// physical bindings only for A/B/X/Y, L/R, Select/Start and the D-Pad, and its
// seed convention is "slot A=south, B=east, X=west, Y=north" — so GC face
// buttons map straight through (south = GC A), matching the no-swap GCPad
// profile the libretro core writes at boot. GC Z borrows the spare seeded
// "Select" slot (physical Back). Analog Main/C sticks are fixed-routed by
// SdlInputManager (not controls.ini-remappable) and so are not listed here;
// they still drive the game via the core's GCPad profile.
QVector<BindingDef> gcPadBindings() {
    return {
        // D-Pad
        { BindingDef::Button, "D-Pad Up",    "D-Pad", "Pad1", "Up",    "SDL-0/DPadUp",    "DPad", 632, 740, 50 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad", "Pad1", "Down",  "SDL-0/DPadDown",  "DPad", 632, 902, 50 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad", "Pad1", "Left",  "SDL-0/DPadLeft",  "DPad", 557, 820, 50 },
        { BindingDef::Button, "D-Pad Right", "D-Pad", "Pad1", "Right", "SDL-0/DPadRight", "DPad", 707, 821, 50 },
        // Face buttons
        { BindingDef::Button, "A", "Face Buttons", "Pad1", "A", "SDL-0/FaceSouth", "FaceButtons", 1430, 438, 90 },
        { BindingDef::Button, "B", "Face Buttons", "Pad1", "B", "SDL-0/FaceEast",  "FaceButtons", 1233, 543, 60 },
        { BindingDef::Button, "X", "Face Buttons", "Pad1", "X", "SDL-0/FaceWest",  "FaceButtons", 1626, 403, 65 },
        { BindingDef::Button, "Y", "Face Buttons", "Pad1", "Y", "SDL-0/FaceNorth", "FaceButtons", 1390, 250, 65 },
        // Z (borrows RetroPad Select — the only spare seeded slot)
        { BindingDef::Button, "Z", "Triggers", "Pad1", "Select", "SDL-0/Back", "Shoulders", 1430, 100, 50 },
        // Shoulders
        { BindingDef::Button, "L", "Triggers", "Pad1", "L", "SDL-0/LeftShoulder",  "Shoulders", 290, 100, 80 },
        { BindingDef::Button, "R", "Triggers", "Pad1", "R", "SDL-0/RightShoulder", "Shoulders", 1517, 78, 80 },
        // System
        { BindingDef::Button, "Start", "System", "Pad1", "Start", "SDL-0/Start", "System", 920, 420, 35 },
    };
}

// Wii Classic Controller. Wii_classiccontroller.svg viewBox is 0 0 2340 1182.
// Display + remap surface only: the libretro core does not yet write a default
// Wiimote/Classic profile, so these do not drive a Wii game end-to-end yet
// (tracked as follow-up). Same RetroPad-slot convention as GC. ZL/ZR/Home have
// no spare seeded slot and are omitted for v1.
QVector<BindingDef> wiiClassicBindings() {
    return {
        // D-Pad
        { BindingDef::Button, "D-Pad Up",    "D-Pad", "Pad1", "Up",    "SDL-0/DPadUp",    "DPad", 461, 344, 50 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad", "Pad1", "Down",  "SDL-0/DPadDown",  "DPad", 461, 574, 50 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad", "Pad1", "Left",  "SDL-0/DPadLeft",  "DPad", 345, 459, 50 },
        { BindingDef::Button, "D-Pad Right", "D-Pad", "Pad1", "Right", "SDL-0/DPadRight", "DPad", 577, 459, 50 },
        // Face buttons
        { BindingDef::Button, "A", "Face Buttons", "Pad1", "A", "SDL-0/FaceSouth", "FaceButtons", 2108, 460, 80 },
        { BindingDef::Button, "B", "Face Buttons", "Pad1", "B", "SDL-0/FaceEast",  "FaceButtons", 1883, 633, 80 },
        { BindingDef::Button, "X", "Face Buttons", "Pad1", "X", "SDL-0/FaceWest",  "FaceButtons", 1883, 289, 80 },
        { BindingDef::Button, "Y", "Face Buttons", "Pad1", "Y", "SDL-0/FaceNorth", "FaceButtons", 1659, 461, 80 },
        // Shoulders
        { BindingDef::Button, "L", "Triggers", "Pad1", "L", "SDL-0/LeftShoulder",  "Shoulders", 370,  80, 70 },
        { BindingDef::Button, "R", "Triggers", "Pad1", "R", "SDL-0/RightShoulder", "Shoulders", 1970, 80, 70 },
        // System
        { BindingDef::Button, "Minus", "System", "Pad1", "Select", "SDL-0/Back",  "System", 996,  459, 50 },
        { BindingDef::Button, "Plus",  "System", "Pad1", "Start",  "SDL-0/Start", "System", 1343, 459, 50 },
    };
}

}  // namespace

QVector<ControllerTypeDef> DolphinLibretroAdapter::controllerTypes() const {
    return {
        { "GCPad1", "GameCube Controller",
          ":/AppUI/qml/AppUI/images/controllers/GameCube.svg" },
        { "WiiClassic", "Wii Classic Controller",
          ":/AppUI/qml/AppUI/images/controllers/Wii_classiccontroller.svg" },
    };
}

QVector<BindingDef> DolphinLibretroAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type == "WiiClassic")
        return wiiClassicBindings();
    return gcPadBindings();  // "GCPad1" or the empty default used by seeding
}
