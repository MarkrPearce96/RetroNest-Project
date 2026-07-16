#include "snes9x_libretro_adapter.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"
#include <QDir>

QVector<PathDef> Snes9xLibretroAdapter::pathsDefs() const {
    return {
        { "Saves",       "libretro", "Saves",      "saves",      PathBase::EmulatorData },
        { "Save States", "libretro", "SaveStates", "savestates", PathBase::EmulatorData },
    };
}

QVector<ControllerTypeDef> Snes9xLibretroAdapter::controllerTypes() const {
    // The SVG art doesn't exist yet — the mapping page renders no controller
    // image until it's added, but the bindings below still drive input.
    return {
        { "SNES", "SNES Controller",
          ":/AppUI/qml/AppUI/images/controllers/SuperNintendo.svg" },
    };
}

QVector<BindingDef> Snes9xLibretroAdapter::controllerBindingDefsForType(const QString&) const {
    // SNES IS the RetroPad reference layout, so coreButton names map 1:1 to
    // RetroPad slots via retroPadSlotFromKey(); the empty defaultValue lets the
    // base ensureConfig() seed the standard SDL elements (A→FaceSouth,
    // B→FaceEast, X→FaceWest, Y→FaceNorth, …). Zero spotlight coords: no SNES
    // controller SVG yet, so no overlay dots (art follow-up).
    return {
        { BindingDef::Button, "D-Pad Up",    "D-Pad",     "Pad1", "Up",     "", "DPad",        0, 0, 0 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",     "Pad1", "Down",   "", "DPad",        0, 0, 0 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",     "Pad1", "Left",   "", "DPad",        0, 0, 0 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",     "Pad1", "Right",  "", "DPad",        0, 0, 0 },
        { BindingDef::Button, "A",           "Buttons",   "Pad1", "A",      "", "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "B",           "Buttons",   "Pad1", "B",      "", "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "X",           "Buttons",   "Pad1", "X",      "", "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "Y",           "Buttons",   "Pad1", "Y",      "", "FaceButtons", 0, 0, 0 },
        { BindingDef::Button, "L",           "Shoulders", "Pad1", "L",      "", "Shoulders",   0, 0, 0 },
        { BindingDef::Button, "R",           "Shoulders", "Pad1", "R",      "", "Shoulders",   0, 0, 0 },
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "", "System",      0, 0, 0 },
        { BindingDef::Button, "Select",      "System",    "Pad1", "Select", "", "System",      0, 0, 0 },
    };
}

QString Snes9xLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    // Mirror the write side (GameSession::terminate / libretroSlotPath): honor
    // a SaveStates path override first, else the per-system default dir.
    const QString override = PathOverridesStore::instance().read("snes9x", "SaveStates");
    if (!override.isEmpty()) {
        QDir d(override);
        const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
        return entries.isEmpty() ? QString() : d.absoluteFilePath(entries.first());
    }
    const QString dir = Paths::emulatorDataDir("snes9x", "snes") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    return entries.isEmpty() ? QString() : d.absoluteFilePath(entries.first());
}
