#include "mupen64plus_libretro_adapter.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"
#include <QDir>

QVector<PathDef> Mupen64PlusLibretroAdapter::pathsDefs() const {
    return {
        { "Saves",       "libretro", "Saves",      "saves",      PathBase::EmulatorData },
        { "Save States", "libretro", "SaveStates", "savestates", PathBase::EmulatorData },
    };
}

QVector<ControllerTypeDef> Mupen64PlusLibretroAdapter::controllerTypes() const {
    // The SVG art doesn't exist yet — the mapping page renders no controller
    // image until it's added, but the bindings below still drive input.
    return {
        { "N64", "N64 Controller",
          ":/AppUI/qml/AppUI/images/controllers/Nintendo64.svg" },
    };
}

QVector<BindingDef> Mupen64PlusLibretroAdapter::controllerBindingDefsForType(const QString&) const {
    // The .key is the RetroPad slot (retroPadSlotFromKey); the .defaultValue is
    // the physical SDL element that drives it. The core then maps that RetroPad
    // slot to the N64 button (see custom/.../emulate_game_controller_via_libretro.c
    // inputGetKeys_default): JOYPAD_B→N64 A, JOYPAD_Y→N64 B, JOYPAD_X→C-Up,
    // JOYPAD_A→C-Down, JOYPAD_L→C-Left, JOYPAD_R→C-Right, JOYPAD_L2→Z,
    // JOYPAD_R2→N64 R, JOYPAD_SELECT→N64 L. The defaults below reproduce the
    // standard RetroArch N64 feel on a modern pad. The N64 analog stick is the
    // host left stick, fixed-routed by SdlInputManager (no controls.ini row).
    return {
        { BindingDef::Button, "D-Pad Up",    "D-Pad",     "Pad1", "Up",     "SDL-0/DPadUp",        "DPad",         0, 0, 0 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",     "Pad1", "Down",   "SDL-0/DPadDown",      "DPad",         0, 0, 0 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",     "Pad1", "Left",   "SDL-0/DPadLeft",      "DPad",         0, 0, 0 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",     "Pad1", "Right",  "SDL-0/DPadRight",     "DPad",         0, 0, 0 },
        { BindingDef::Button, "A",           "Buttons",   "Pad1", "B",      "SDL-0/FaceSouth",     "FaceButtons",  0, 0, 0 },
        { BindingDef::Button, "B",           "Buttons",   "Pad1", "Y",      "SDL-0/FaceWest",      "FaceButtons",  0, 0, 0 },
        { BindingDef::Button, "C-Up",        "C Buttons", "Pad1", "X",      "SDL-0/FaceNorth",     "RightAnalog",  0, 0, 0 },
        { BindingDef::Button, "C-Down",      "C Buttons", "Pad1", "A",      "SDL-0/FaceEast",      "RightAnalog",  0, 0, 0 },
        { BindingDef::Button, "C-Left",      "C Buttons", "Pad1", "L",      "SDL-0/LeftShoulder",  "RightAnalog",  0, 0, 0 },
        { BindingDef::Button, "C-Right",     "C Buttons", "Pad1", "R",      "SDL-0/RightShoulder", "RightAnalog",  0, 0, 0 },
        { BindingDef::Button, "Z Trigger",   "Triggers",  "Pad1", "L2",     "SDL-0/+LeftTrigger",  "Shoulders",    0, 0, 0 },
        { BindingDef::Button, "L",           "Triggers",  "Pad1", "Select", "SDL-0/Back",          "Shoulders",    0, 0, 0 },
        { BindingDef::Button, "R",           "Triggers",  "Pad1", "R2",     "SDL-0/+RightTrigger", "Shoulders",    0, 0, 0 },
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "SDL-0/Start",         "System",       0, 0, 0 },
    };
}

QString Mupen64PlusLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    // Mirror the write side (GameSession::terminate / libretroSlotPath): honor
    // a SaveStates path override first, else the per-system default dir.
    const QString override = PathOverridesStore::instance().read("mupen64plus", "SaveStates");
    if (!override.isEmpty()) {
        QDir d(override);
        const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
        return entries.isEmpty() ? QString() : d.absoluteFilePath(entries.first());
    }
    const QString dir = Paths::emulatorDataDir("mupen64plus", "n64") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    return entries.isEmpty() ? QString() : d.absoluteFilePath(entries.first());
}
