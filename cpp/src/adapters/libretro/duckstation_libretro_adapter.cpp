#include "duckstation_libretro_adapter.h"

#include "core/binding_def.h"
#include "core/controller_type_def.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"

#include <QDir>

// PS1 Digital Controller — the standard (non-analog) PlayStation pad the
// DuckStation libretro core boots with. No PS1-specific controller SVG ships
// under :/AppUI/qml/AppUI/images/controllers/ yet, so svgResource is empty —
// ControllerBindingsView renders without artwork (spotlight coords are all
// 0/0/0 below, i.e. "no spotlight"). A future SP can add a PS1 pad SVG.
QVector<ControllerTypeDef> DuckStationLibretroAdapter::controllerTypes() const {
    return {
        {"DigitalController", "Digital Controller", ""},
    };
}

// PS1 digital pad binding defs.
//
// Action keys (the .key field) match retroPadSlotFromKey() (input_router.h)
// — Up/Down/Left/Right/B/A/Y/X/L/R/L2/R2/Start/Select — so GameSession's
// controls.ini parser resolves each line to a RetroPadSlot and binds it into
// the InputRouter. Default values follow the SDL-0/... convention
// LibretroAdapter::ensureConfig seeds the file with on first launch. Both
// slots and defaults are identical to Pcsx2LibretroAdapter (same RetroPad
// layout); only the labels carry PS1 button names.
//
// PS1 face button positions vs libretro RetroPad:
//   RetroPad B  (south) = PS1 Cross
//   RetroPad A  (east)  = PS1 Circle
//   RetroPad Y  (west)  = PS1 Square
//   RetroPad X  (north) = PS1 Triangle
//
// A digital pad has no analog sticks, so L3/R3 are omitted. Spotlight coords
// are 0/0/0 (no spotlight) because there is no PS1 controller SVG yet.
QVector<BindingDef> DuckStationLibretroAdapter::controllerBindingDefsForType(const QString&) const {
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
        { BindingDef::Button, "L1", "Shoulders", "Pad1", "L",  "SDL-0/LeftShoulder",  "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R1", "Shoulders", "Pad1", "R",  "SDL-0/RightShoulder", "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "L2", "Shoulders", "Pad1", "L2", "SDL-0/+LeftTrigger",  "Shoulders", 0, 0, 0 },
        { BindingDef::Button, "R2", "Shoulders", "Pad1", "R2", "SDL-0/+RightTrigger", "Shoulders", 0, 0, 0 },
        // System
        { BindingDef::Button, "Start",  "System", "Pad1", "Start",  "SDL-0/Start", "System", 0, 0, 0 },
        { BindingDef::Button, "Select", "System", "Pad1", "Select", "SDL-0/Back",  "System", 0, 0, 0 },
    };
}

QVector<PathDef> DuckStationLibretroAdapter::pathsDefs() const {
    return {
        { "Memory Cards", "libretro", "MemoryCards", "memcards",   PathBase::EmulatorData },
        { "Save States",  "libretro", "SaveStates",  "savestates", PathBase::EmulatorData },
    };
}

// Resume-on-launch: GameSession::terminate writes "<serial>.resume" under the
// DuckStation SaveStates dir; locate it here so GameSession feeds it to
// cfg.resumeStatePath (loaded post-retro_load_game via retro_unserialize).
// Base id "duckstation", systemId "psx". Mirrors Pcsx2LibretroAdapter::findResumeFile.
QString DuckStationLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    QString dir = PathOverridesStore::instance().read("duckstation", "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir("duckstation", "psx") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    if (!entries.isEmpty())
        return d.absoluteFilePath(entries.first());
    return {};
}
