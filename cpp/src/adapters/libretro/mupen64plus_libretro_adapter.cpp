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
    // Spotlight coordinates are in the SVG's viewBox coordinate SYSTEM but
    // relative to the viewBox ORIGIN (the view maps spotlightX*scale without
    // subtracting vb.x/y — all controller SVGs before this one had 0,0
    // origins). Nintendo64.svg's viewBox is "38 87 425 388", so these are
    // (imageX-38, imageY-87) of button centers measured by color-clustering
    // the artwork (C cluster = yellow, A = blue, B = green, Start = red,
    // D-pad = dark cross; Z is the underside trigger — spotlit at the center
    // prong; L/R sit on the top edge).
    return {
        { BindingDef::Button, "D-Pad Up",    "D-Pad",     "Pad1", "Up",     "SDL-0/DPadUp",        "DPad",         89, 106, 13 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",     "Pad1", "Down",   "SDL-0/DPadDown",      "DPad",         89, 144, 13 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",     "Pad1", "Left",   "SDL-0/DPadLeft",      "DPad",         70, 125, 13 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",     "Pad1", "Right",  "SDL-0/DPadRight",     "DPad",        108, 125, 13 },
        { BindingDef::Button, "A",           "Buttons",   "Pad1", "B",      "SDL-0/FaceSouth",     "FaceButtons", 314, 166, 16 },
        { BindingDef::Button, "B",           "Buttons",   "Pad1", "Y",      "SDL-0/FaceWest",      "FaceButtons", 287, 140, 16 },
        { BindingDef::Button, "C-Up",        "C Buttons", "Pad1", "X",      "SDL-0/FaceNorth",     "RightAnalog", 345,  91, 13 },
        { BindingDef::Button, "C-Down",      "C Buttons", "Pad1", "A",      "SDL-0/FaceEast",      "RightAnalog", 345, 138, 13 },
        { BindingDef::Button, "C-Left",      "C Buttons", "Pad1", "L",      "SDL-0/LeftShoulder",  "RightAnalog", 321, 115, 13 },
        { BindingDef::Button, "C-Right",     "C Buttons", "Pad1", "R",      "SDL-0/RightShoulder", "RightAnalog", 369, 115, 13 },
        { BindingDef::Button, "Z Trigger",   "Triggers",  "Pad1", "L2",     "SDL-0/+LeftTrigger",  "Shoulders",   211, 258, 18 },
        { BindingDef::Button, "L",           "Triggers",  "Pad1", "Select", "SDL-0/Back",          "Shoulders",    72,  58, 16 },
        { BindingDef::Button, "R",           "Triggers",  "Pad1", "R2",     "SDL-0/+RightTrigger", "Shoulders",   350,  58, 16 },
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "SDL-0/Start",         "System",      211, 143, 16 },
    };
}

// Schema curation: the core declares (keys/values/defaults/labels via the
// declared_options.json sidecar), this overlay only routes options into UI
// categories. Deliberately HIDDEN (uncurated — valid in options.json but no
// UI row): cpucore (footgun — seeding bugs already bit us once), the legacy
// 43screensize/169screensize fixed-size lists (redundant with — and only
// active without — the native-res factor, which we default to the
// equivalent 2x so the fallback never engages), rdp/rsp
// plugin selection (GLideN64+HLE is the only shipped config; parallel/
// angrylion aren't built), ThreadedRenderer, CountPerOp*, Framerate,
// IgnoreTLBExceptions, FrameDuping (frontend paces), Overscan* (the overscan
// pass is disabled on macOS — see the fork's Config_mupenplus), FBEmulation +
// RDRAM-copy accuracy toggles (load-bearing for the present path), and the
// pak2-4 / c-button remaps (multiplayer + niche remapping, revisit with
// multi-pad support).
QVector<OptionOverlay> Mupen64PlusLibretroAdapter::optionOverlays() const {
    auto ov = [](const QString& key, const QStringList& categories,
                 const QString& dependsOn = QString()) {
        OptionOverlay o;
        o.key = key;
        for (const auto& c : categories)
            o.placements.append({ c, {}, {} });
        o.dependsOn = dependsOn;
        return o;
    };
    // Deliberate RetroNest default (rare defaultOverride): factor "2" is the
    // same effective quality as the core's stock fallback (factor 0 →
    // 43screensize 640x480 = 2x native 320x240), but keeps the UI out of the
    // "Disabled" state whose behavior lives in the hidden legacy size lists.
    OptionOverlay resFactor = ov("mupen64plus-EnableNativeResFactor",
                                 { "Recommended", "Graphics" });
    resFactor.defaultOverride = "2";
    // "0"/Disabled is a dead end here (its behavior lives in the hidden
    // legacy size lists) — trim it from the UI row.
    resFactor.excludedValues = { "0" };
    return {
        // Recommended (cross-listed; same OptionsStore keys)
        resFactor,
        ov("mupen64plus-aspect",                { "Recommended", "Graphics" }),
        ov("mupen64plus-MultiSampling",         { "Recommended", "Graphics" }),
        ov("mupen64plus-BilinearMode",          { "Recommended", "Graphics" }),
        ov("mupen64plus-FXAA",              { "Graphics" }),
        ov("mupen64plus-HybridFilter",      { "Graphics" }),
        ov("mupen64plus-EnableHWLighting",  { "Graphics" }),
        ov("mupen64plus-EnableLODEmulation",{ "Graphics" }),
        // Textures
        ov("mupen64plus-txFilterMode",      { "Textures" }),
        ov("mupen64plus-txEnhancementMode", { "Textures" }),
        ov("mupen64plus-txHiresEnable",     { "Textures" }),
        // Input
        ov("mupen64plus-astick-deadzone",    { "Input" }),
        ov("mupen64plus-astick-sensitivity", { "Input" }),
        ov("mupen64plus-pak1",               { "Input" }),
    };
}

QVector<SettingsHubCard> Mupen64PlusLibretroAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width curated card (fleet convention).
        {QStringLiteral("\U0001F4A1"), "Recommended",
         "Most-tweaked settings — resolution, aspect, anti-aliasing",
         "Recommended", 0, 0, 1, 3},
        // Row 1: Graphics · Textures · Input
        {QStringLiteral("\U0001F5BC"), "Graphics",
         "Resolution, aspect ratio, filtering, lighting",
         "Graphics", 1, 0},
        {QStringLiteral("\U0001F3A8"), "Textures",
         "Texture filtering and enhancement",
         "Textures", 1, 1},
        {QStringLiteral("\U0001F3AE"), "Input",
         "Analog stick response, controller pak",
         "Input", 1, 2},
    };
}

PreviewSpec Mupen64PlusLibretroAdapter::previewSpec(const QString& category,
                                                    const QString& subcategory) const {
    if (category == "Recommended" && subcategory.isEmpty()) {
        // Aspect-ratio preview pane on the Recommended page — mirrors
        // PCSX2/DuckStation/Dolphin. Bound to the same mupen64plus-aspect
        // key the Recommended card lists ("4:3" / "16:9" / "16:9 adjusted";
        // AspectRatioPreview::fromSchemaValue maps all three).
        return {"aspect", {
            {"mupen64plus-aspect", "aspectMode"},
        }};
    }
    return {};
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
