#include "pcsx2_libretro_adapter.h"

#include "core/binding_def.h"

// SP5: PS2 DualShock 2 binding defs.
//
// Action keys (the .key field) match retroPadSlotFromKey() (input_router.h)
// — B/Y/Select/Start/Up/Down/Left/Right/A/X/L/R/L2/R2/L3/R3 — so
// GameSession's controls.ini parser resolves each line to a RetroPadSlot
// and binds it into the InputRouter. Default values follow the SDL-0/...
// convention LibretroAdapter::ensureConfig seeds the file with on first launch.
//
// PS2 face button positions vs libretro RetroPad:
//   RetroPad B  (south) = PS2 Cross
//   RetroPad A  (east)  = PS2 Circle
//   RetroPad Y  (west)  = PS2 Square
//   RetroPad X  (north) = PS2 Triangle
//
// Spotlight coords are 0/0/0 (no spotlight) because there is no PS2
// controller SVG configured yet. Future SP can add one.
QVector<BindingDef> Pcsx2LibretroAdapter::controllerBindingDefsForType(const QString&) const {
    return {
        // D-Pad
        { BindingDef::Button, "D-Pad Up",    "D-Pad", "Pad1", "Up",    "SDL-0/DPadUp",    "DPad", 0, 0, 0 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad", "Pad1", "Down",  "SDL-0/DPadDown",  "DPad", 0, 0, 0 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad", "Pad1", "Left",  "SDL-0/DPadLeft",  "DPad", 0, 0, 0 },
        { BindingDef::Button, "D-Pad Right", "D-Pad", "Pad1", "Right", "SDL-0/DPadRight", "DPad", 0, 0, 0 },
        // Face buttons
        { BindingDef::Button, "Cross",    "Buttons", "Pad1", "B", "SDL-0/FaceSouth", "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "Circle",   "Buttons", "Pad1", "A", "SDL-0/FaceEast",  "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "Square",   "Buttons", "Pad1", "Y", "SDL-0/FaceWest",  "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "Triangle", "Buttons", "Pad1", "X", "SDL-0/FaceNorth", "FaceButtons", 0, 0, 0 },
        // Shoulders + triggers (triggers route as digital here; full analog is future work)
        { BindingDef::Button, "L1", "Shoulders", "Pad1", "L",  "SDL-0/LeftShoulder",   "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R1", "Shoulders", "Pad1", "R",  "SDL-0/RightShoulder",  "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "L2", "Shoulders", "Pad1", "L2", "SDL-0/+LeftTrigger",   "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R2", "Shoulders", "Pad1", "R2", "SDL-0/+RightTrigger",  "Shoulders", 0, 0, 0 },
        // Stick clicks
        { BindingDef::Button, "L3 (Left Stick Click)",  "Sticks", "Pad1", "L3", "SDL-0/LeftStick",  "LeftAnalog",  0, 0, 0 },
        { BindingDef::Button, "R3 (Right Stick Click)", "Sticks", "Pad1", "R3", "SDL-0/RightStick", "RightAnalog", 0, 0, 0 },
        // System
        { BindingDef::Button, "Start",  "System", "Pad1", "Start",  "SDL-0/Start", "System", 0, 0, 0 },
        { BindingDef::Button, "Select", "System", "Pad1", "Select", "SDL-0/Back",  "System", 0, 0, 0 },
    };
}
