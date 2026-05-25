#include "dolphin_libretro_adapter.h"

#include "core/ini_file.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>

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
        // GC L/R are analog triggers, so they default to the controller's analog
        // triggers; Z defaults to the right shoulder.  (RetroPad slots stay L/R
        // for L/R and Select for Z — only the physical-input default differs.)
        { BindingDef::Button, "Z", "Triggers", "Pad1", "Select", "SDL-0/RightShoulder", "Shoulders", 1430, 100, 50 },
        { BindingDef::Button, "L", "Triggers", "Pad1", "L", "SDL-0/+LeftTrigger",  "Shoulders", 290, 100, 80 },
        { BindingDef::Button, "R", "Triggers", "Pad1", "R", "SDL-0/+RightTrigger", "Shoulders", 1517, 78, 80 },
        // System
        { BindingDef::Button, "Start", "System", "Pad1", "Start", "SDL-0/Start", "System", 920, 420, 35 },
    };
}

// Wii Classic Controller. Wii_classiccontroller.svg viewBox is 0 0 2340 1182.
// RetroPad-slot keys must match the core's WiimoteNew.ini Classic profile + the
// feed set below (they share [Pad1]): ZL<-L2, ZR<-Select (shared with GC Z),
// minus<-R2, plus<-Start, Home<-R3; L/R analog<-L/R (the analog triggers).
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
        // Shoulders / triggers
        { BindingDef::Button, "L",  "Triggers", "Pad1", "L",      "SDL-0/+LeftTrigger",  "Shoulders", 370,  80, 70 },
        { BindingDef::Button, "ZL", "Triggers", "Pad1", "L2",     "SDL-0/LeftShoulder",  "Shoulders", 570,  80, 60 },
        { BindingDef::Button, "R",  "Triggers", "Pad1", "R",      "SDL-0/+RightTrigger", "Shoulders", 1970, 80, 70 },
        { BindingDef::Button, "ZR", "Triggers", "Pad1", "Select", "SDL-0/RightShoulder", "Shoulders", 1770, 80, 60 },
        // System
        { BindingDef::Button, "Minus", "System", "Pad1", "R2",    "SDL-0/Back",  "System", 996,  459, 50 },
        { BindingDef::Button, "Home",  "System", "Pad1", "R3",    "SDL-0/Guide", "System", 1170, 545, 45 },
        { BindingDef::Button, "Plus",  "System", "Pad1", "Start", "SDL-0/Start", "System", 1343, 459, 50 },
    };
}

// Device-level feed/seed set (the empty controller-type fallback). ensureConfig
// seeds controls.ini from this and game_session binds the InputRouter from it,
// so it must be the UNION of every RetroPad slot either controller needs. It is
// the GameCube layout plus the Wii-Classic-only slots (ZL/minus/Home -> L2/R2/
// R3); Select/Start are already fed by GameCube (Z / Start). Not shown on any
// mapping page (spotlights are 0). Physical defaults match wiiClassicBindings().
QVector<BindingDef> feedBindings() {
    auto defs = gcPadBindings();
    defs.append({ BindingDef::Button, "ZL",    "Triggers", "Pad1", "L2", "SDL-0/LeftShoulder", "Shoulders", 0, 0, 0 });
    defs.append({ BindingDef::Button, "Minus", "System",   "Pad1", "R2", "SDL-0/Back",         "System",    0, 0, 0 });
    defs.append({ BindingDef::Button, "Home",  "System",   "Pad1", "R3", "SDL-0/Guide",        "System",    0, 0, 0 });
    return defs;
}

}  // namespace

QVector<ControllerTypeDef> DolphinLibretroAdapter::controllerTypes() const {
    return {
        { "GCPad1", "GameCube Controller",
          ":/AppUI/qml/AppUI/images/controllers/GameCube.svg" },
        { "Wiimote1", "Wii Classic Controller",
          ":/AppUI/qml/AppUI/images/controllers/Wii_classiccontroller.svg" },
    };
}

QVector<BindingDef> DolphinLibretroAdapter::controllerBindingDefsForType(const QString& type) const {
    // Type ids match EmulatorDetailPage.qml + Dolphin's native section naming
    // ("GCPad1" / "Wiimote1"). A mismatch here makes ControllerBindingsView
    // deref a null matchedType (its Q_ASSERT is a no-op in Release) and crash.
    if (type == "GCPad1")
        return gcPadBindings();
    if (type == "Wiimote1")
        return wiiClassicBindings();
    // Empty type = ensureConfig seeding + game_session InputRouter feed: return
    // the device feed superset so the Wii-only slots (ZL/ZR/Home) are fed too.
    return feedBindings();
}

// SP6: Graphics core-options schema — 53 rows across five sub-tabs.
// Keys, defaults, and value-sets mirror CoreOptionsGraphics.cpp exactly;
// the schema-fidelity tool (tools/check_schema_fidelity.py) enforces this.
QVector<SettingDef> DolphinLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    auto gopt = [](const QString& subcategory, const QString& group,
                   const QString& key, const QString& label, const QString& def,
                   const QVector<QPair<QString,QString>>& valuesAndLabels,
                   const QString& tooltip, const QString& dependsOn = {}) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = "Graphics";
        d.subcategory = subcategory;
        d.group = group;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.tooltip = tooltip;
        d.type = SettingDef::Combo;
        d.options = valuesAndLabels;
        d.dependsOn = dependsOn;
        return d;
    };

    // opt(): like gopt but with an explicit category (subcategory empty) for
    // the flat (no sub-tab) categories — Audio/General/Advanced/GameCube/Wii
    // and the Recommended view. Mirrors pcsx2_libretro_adapter's opt().
    auto opt = [](const QString& category, const QString& group,
                  const QString& key, const QString& label, const QString& def,
                  const QVector<QPair<QString,QString>>& valuesAndLabels,
                  const QString& tooltip, const QString& dependsOn = {}) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.category = category;
        d.subcategory = "";
        d.group = group;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.tooltip = tooltip;
        d.type = SettingDef::Combo;
        d.options = valuesAndLabels;
        d.dependsOn = dependsOn;
        return d;
    };

    // ── General ──────────────────────────────────────────────────────────
    s.append(gopt("General", "General", "dolphin_aspect_ratio", "Aspect Ratio", "Auto",
        {{"Auto","Auto"},{"Force 16:9","16:9"},{"Force 4:3","4:3"},{"Stretch to Window","Stretch"}},
        "Display aspect ratio. Auto matches the game's native aspect; Stretch fills the window."));
    s.append(gopt("General", "General", "dolphin_vsync", "V-Sync", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Synchronizes output to the display refresh rate. Reduces tearing."));
    s.append(gopt("General", "General", "dolphin_precision_frame_timing", "Precision Frame Timing", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Uses high-resolution timers and busy-waiting for improved frame "
        "pacing. Slightly higher power use."));
    s.append(gopt("General", "General", "dolphin_shader_compilation", "Shader Compilation", "Specialized",
        {{"Specialized (Default)","Specialized"},{"Exclusive Ubershaders","Exclusive Ubershaders"},
         {"Hybrid Ubershaders","Hybrid Ubershaders"},{"Skip Drawing","Skip Drawing"}},
        "How shaders are compiled. Ubershader modes reduce stutter at a GPU "
        "cost; Skip Drawing is for debugging."));
    s.append(gopt("General", "General", "dolphin_wait_for_shaders", "Compile Shaders Before Starting", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pre-compile the shader pipeline before launching. Slower start, "
        "smoother first minutes of gameplay."));

    // ── Enhancements ─────────────────────────────────────────────────────
    s.append(gopt("Enhancements", "Enhancements", "dolphin_internal_resolution", "Internal Resolution", "1x",
        {{"Auto (Window Size)","Auto"},{"Native (1x)","1x"},{"2x","2x"},{"3x","3x"},{"4x","4x"},
         {"5x","5x"},{"6x (4K)","6x"},{"7x","7x"},{"8x","8x"}},
        "Render scale relative to native (1x = 640x528; 6x is roughly 4K)."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_antialiasing", "Anti-Aliasing", "None",
        {{"None","None"},{"2x MSAA","2x MSAA"},{"4x MSAA","4x MSAA"},{"8x MSAA","8x MSAA"},
         {"2x SSAA","2x SSAA"},{"4x SSAA","4x SSAA"},{"8x SSAA","8x SSAA"}},
        "Reduces aliasing on edges. SSAA is far more demanding than MSAA "
        "but also anti-aliases shader effects."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_texture_filtering", "Texture Filtering", "Default",
        {{"Default","Default"},
         {"1x Anisotropic","1x Anisotropic"},{"2x Anisotropic","2x Anisotropic"},
         {"4x Anisotropic","4x Anisotropic"},{"8x Anisotropic","8x Anisotropic"},
         {"16x Anisotropic","16x Anisotropic"},
         {"Force Nearest and 1x Anisotropic","Force Nearest and 1x Anisotropic"},
         {"Force Linear and 1x Anisotropic","Force Linear and 1x Anisotropic"},
         {"Force Linear and 2x Anisotropic","Force Linear and 2x Anisotropic"},
         {"Force Linear and 4x Anisotropic","Force Linear and 4x Anisotropic"},
         {"Force Linear and 8x Anisotropic","Force Linear and 8x Anisotropic"},
         {"Force Linear and 16x Anisotropic","Force Linear and 16x Anisotropic"}},
        "Sharpens distant textures (anisotropic) and optionally forces a "
        "fixed magnification filter."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_output_resampling", "Output Resampling", "Default",
        {{"Default","Default"},{"Bilinear","Bilinear"},{"Bicubic: B-Spline","Bicubic B-Spline"},
         {"Bicubic: Mitchell-Netravali","Bicubic Mitchell-Netravali"},{"Bicubic: Catmull-Rom","Bicubic Catmull-Rom"},
         {"Sharp Bilinear","Sharp Bilinear"},{"Area Sampling","Area Sampling"}},
        "Algorithm used to resample the rendered image to the window size."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_scaled_efb_copy", "Scaled EFB Copy", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Resize EFB copies to match the rendering scale. Required for high internal resolutions to look right."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_per_pixel_lighting", "Per-Pixel Lighting", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Higher-quality lighting at a small performance cost."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_widescreen_hack", "Widescreen Hack", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Force 4:3 games to render in widescreen by hacking the projection matrix. Can produce artifacts."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_force_true_color", "Force 24-Bit Color", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Force higher-precision color output. Reduces banding on gradients."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_disable_fog", "Disable Fog", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip rendering fog effects."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_arbitrary_mipmap_detection", "Arbitrary Mipmap Detection", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Detect when a game uses mipmaps as separate images rather than true LODs."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_disable_copy_filter", "Disable Copy Filter", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Disable the post-process copy-filter pass. Reduces blur some games apply."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_hdr_output", "HDR Post-Processing", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Output in HDR when the display supports it."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_stereo_mode", "Stereoscopic 3D Mode", "Off",
        {{"Off","Off"},{"Side-by-Side","Side-by-Side"},{"Top-and-Bottom","Top-and-Bottom"},
         {"Anaglyph","Anaglyph"},{"HDMI 3D","HDMI 3D"},{"Passive","Passive"}},
        "3D-stereoscopic rendering mode. Off disables stereo entirely."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_stereo_swap_eyes", "Swap Eyes", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Swap the left and right eye images."));
    s.append(gopt("Enhancements", "Enhancements", "dolphin_stereo_per_eye_full", "Full Resolution Per Eye", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Render each eye at the full internal resolution instead of half. Doubles GPU cost."));

    // ── Hacks ─────────────────────────────────────────────────────────────
    s.append(gopt("Hacks", "Hacks", "dolphin_texcache_accuracy", "Texture Cache Accuracy", "Default",
        {{"Safe","Safe"},{"Default","Default"},{"Fast","Fast"}},
        "How aggressively cached textures are validated. Safe = fewest "
        "misses (most accurate), Fast = highest performance."));
    s.append(gopt("Hacks", "Hacks", "dolphin_skip_efb_access", "Skip EFB Access from CPU", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Ignore CPU reads/writes of the EFB. Speed boost; disables some EFB-based effects."));
    s.append(gopt("Hacks", "Hacks", "dolphin_ignore_format_changes", "Ignore Format Changes", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Ignore EFB format changes. Speed win for many games; minor defects in a few."));
    s.append(gopt("Hacks", "Hacks", "dolphin_store_efb_to_texture", "Store EFB Copies to Texture Only", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Keep EFB copies on the GPU, bypassing RAM. Big speed boost; rare defects."));
    s.append(gopt("Hacks", "Hacks", "dolphin_defer_efb_copies", "Defer EFB Copies to RAM", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Wait for GPU sync before writing EFB copies to RAM. Speed boost."));
    s.append(gopt("Hacks", "Hacks", "dolphin_gpu_texture_decoding", "GPU Texture Decoding", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Decode textures on the GPU instead of the CPU."));
    s.append(gopt("Hacks", "Hacks", "dolphin_store_xfb_to_texture", "Store XFB Copies to Texture Only", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Keep XFB copies on the GPU. Big speed boost; rare defects."));
    s.append(gopt("Hacks", "Hacks", "dolphin_immediate_xfb", "Immediately Present XFB", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Display the XFB as soon as it's drawn. Lower latency, slight tearing risk."));
    s.append(gopt("Hacks", "Hacks", "dolphin_skip_duplicate_xfbs", "Skip Presenting Duplicate Frames", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Detect and skip identical consecutive frames to save GPU work."));
    s.append(gopt("Hacks", "Hacks", "dolphin_fast_depth_calc", "Fast Depth Calculation", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Use a faster GPU-friendly depth calculation path."));
    s.append(gopt("Hacks", "Hacks", "dolphin_disable_bounding_box", "Disable Bounding Box", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Disable bounding-box emulation. Big GPU speed-up; a few games need it (e.g. Paper Mario)."));
    s.append(gopt("Hacks", "Hacks", "dolphin_vertex_rounding", "Vertex Rounding", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Round vertex coordinates to integers. Fixes seams in some games at high resolutions."));
    s.append(gopt("Hacks", "Hacks", "dolphin_save_texcache_to_state", "Save Texture Cache to State", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Save the texture cache in save states. Larger states, smoother resume."));
    s.append(gopt("Hacks", "Hacks", "dolphin_vbi_skip", "VBI Skip", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip Vertical Blank Interrupts when lag is detected. Smoother audio off-100%; can freeze."));

    // ── Advanced ─────────────────────────────────────────────────────────
    s.append(gopt("Advanced", "Advanced", "dolphin_load_custom_textures", "Load Custom Textures", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Load high-resolution texture replacements from the user's Load/Textures folder."));
    s.append(gopt("Advanced", "Advanced", "dolphin_prefetch_custom_textures", "Prefetch Custom Textures", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pre-load all custom textures into VRAM at boot. Eliminates load stutter; uses more memory."));
    s.append(gopt("Advanced", "Advanced", "dolphin_enable_graphics_mods", "Enable Graphics Mods", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Load graphics mods from the user's Load/GraphicMods folder."));
    s.append(gopt("Advanced", "Advanced", "dolphin_crop", "Crop", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Crop overscan/black borders from the rendered image."));
    s.append(gopt("Advanced", "Advanced", "dolphin_backend_multithreading", "Backend Multithreading", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Distribute video-backend work across multiple threads. Recommended on."));
    s.append(gopt("Advanced", "Advanced", "dolphin_prefer_vs_expansion", "Prefer VS for Point/Line Expansion", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Expand line/point primitives in the vertex shader instead of the geometry shader. Driver workaround."));
    s.append(gopt("Advanced", "Advanced", "dolphin_cpu_cull", "Cull Vertices on the CPU", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Cull invisible geometry on the CPU before sending to the GPU. Speeds up some games."));
    s.append(gopt("Advanced", "Advanced", "dolphin_defer_efb_invalidation", "Defer EFB Cache Invalidation", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Reduce overhead by deferring EFB-cache invalidations. Speed win; rare glitches."));
    s.append(gopt("Advanced", "Advanced", "dolphin_manual_texture_sampling", "Manual Texture Sampling", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Trade some speed for accuracy in the texture sampler. (Checked = manual; disables fast sampling.)"));

    // ── On-Screen Display ─────────────────────────────────────────────────
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_osd_font_size", "On-Screen Message Font Size", "13",
        {{"Small","13"},{"Medium","18"},{"Large","24"},{"Extra Large","36"}},
        "Point size for on-screen messages."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_perf_samp_window", "Performance Sample Window", "1000",
        {{"250 ms","250"},{"500 ms","500"},{"1000 ms","1000"},{"2000 ms","2000"},{"5000 ms","5000"}},
        "Sliding window for FPS/VPS averaging. Higher = more stable, slower to update."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_osd_messages", "Show On-Screen Messages", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Display Dolphin's own status messages (save states, achievements, etc.)."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_show_fps", "Show FPS", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Frames per second the GPU is drawing."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_show_ftimes", "Show Frame Times", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Per-frame GPU time graph."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_show_vps", "Show VPS", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "VBlanks per second — the rate the game thinks it's running at."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_show_vtimes", "Show VBlank Times", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Per-vblank time graph."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_show_speed", "Show % Speed", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Emulation speed as a percentage of native."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_show_graphs", "Show Performance Graphs", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Render the FPS/VPS history as a graph."));
    s.append(gopt("On-Screen Display", "On-Screen Display", "dolphin_show_speed_colors", "Show Speed Colors", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Tint the speed indicator based on how close to native it is."));

    // ═══ Audio ═══
    s.append(opt("Audio", "DSP", "dolphin_dsp_engine", "DSP Emulation Engine", "HLE",
        {{"HLE (Recommended)","HLE"},{"LLE Recompiler (Slow)","LLE Recompiler"},{"LLE Interpreter (Very Slow)","LLE Interpreter"}},
        "How the audio DSP is emulated. HLE is fast and compatible; LLE is slower but accurate and required by a few games."));
    s.append(opt("Audio", "Backend", "dolphin_audio_latency", "Audio Latency", "20",
        {{"0 ms","0"},{"10 ms","10"},{"20 ms","20"},{"40 ms","40"},{"60 ms","60"},{"80 ms","80"},{"100 ms","100"},{"150 ms","150"},{"200 ms","200"}},
        "Output latency in milliseconds. Only active with backends that support latency control (OpenAL)."));
    s.append(opt("Audio", "Backend", "dolphin_dpl2_decoder", "Dolby Pro Logic II Decoder", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Decode the stereo mix into 5.1 surround. Requires a DPL2-capable backend and DSP in LLE mode."));
    s.append(opt("Audio", "Backend", "dolphin_dpl2_quality", "DPL2 Decoding Quality", "2",
        {{"Lowest (Latency ~10 ms)","0"},{"Low (Latency ~20 ms)","1"},{"High (Latency ~40 ms)","2"},{"Highest (Latency ~80 ms)","3"}},
        "Trade-off between CPU cost and surround-decode accuracy."));
    s.append(opt("Audio", "Playback", "dolphin_audio_buffer_size", "Audio Buffer Size", "80",
        {{"32 ms","32"},{"48 ms","48"},{"64 ms","64"},{"80 ms","80"},{"96 ms","96"},{"128 ms","128"},{"160 ms","160"},{"256 ms","256"},{"512 ms","512"}},
        "Internal mixer buffer in milliseconds. Higher is smoother but adds delay between picture and sound."));
    s.append(opt("Audio", "Playback", "dolphin_audio_fill_gaps", "Fill Audio Gaps", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Synthesize silence when emulation can't keep up. Disable for accuracy; enable for smoothness."));
    s.append(opt("Audio", "Playback", "dolphin_audio_preserve_pitch", "Preserve Audio Pitch", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Time-stretch audio to keep pitch constant when emulation runs off 100%. Useful with fast-forward."));
    s.append(opt("Audio", "Playback", "dolphin_audio_mute_on_unthrottle", "Mute When Unthrottled", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Silence audio while running unthrottled (fast-forward). Avoids pitch/playback artifacts."));
    s.append(opt("Audio", "Volume", "dolphin_volume", "Volume", "100",
        {{"0%","0"},{"10%","10"},{"20%","20"},{"30%","30"},{"40%","40"},{"50%","50"},{"60%","60"},{"70%","70"},{"80%","80"},{"90%","90"},{"100%","100"}},
        "Master output volume."));

    // ═══ General ═══
    s.append(opt("General", "Basic", "dolphin_cpu_thread", "Dual Core (Speed Hack)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Run CPU and GPU emulation on separate threads. Big speed gain; a few timing-sensitive games may glitch with it on."));
    s.append(opt("General", "Basic", "dolphin_enable_cheats", "Enable Cheats", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Process AR/Gecko cheat codes. Off by default for safety."));
    s.append(opt("General", "Basic", "dolphin_load_game_into_memory", "Load Whole Game Into Memory", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pre-load the entire disc image into RAM at boot. Eliminates disc I/O stutter; uses more host memory."));
    s.append(opt("General", "Basic", "dolphin_override_region_settings", "Allow Mismatched Region Settings", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Force a region's settings (language, video mode) regardless of disc region."));
    s.append(opt("General", "Basic", "dolphin_emulation_speed", "Speed Limit", "1.000000",
        {{"Unlimited","0.000000"},{"10%","0.100000"},{"20%","0.200000"},{"30%","0.300000"},{"40%","0.400000"},{"50%","0.500000"},{"60%","0.600000"},{"70%","0.700000"},{"80%","0.800000"},{"90%","0.900000"},{"100% (Normal Speed)","1.000000"},{"110%","1.100000"},{"120%","1.200000"},{"130%","1.300000"},{"140%","1.400000"},{"150%","1.500000"},{"160%","1.600000"},{"170%","1.700000"},{"180%","1.800000"},{"190%","1.900000"},{"200%","2.000000"}},
        "Cap on emulated speed relative to native. Unlimited removes the throttle."));
    s.append(opt("General", "Region", "dolphin_fallback_region", "Fallback Region", "1",
        {{"NTSC-J (Japan)","0"},{"NTSC-U (Americas)","1"},{"PAL (Europe)","2"},{"Region-Free / Unknown","3"},{"NTSC-K (Korea)","4"}},
        "Region used for games whose region can't be auto-detected. Affects boot timing and the system-menu locale."));

    // ═══ Advanced ═══
    s.append(opt("Advanced", "CPU", "dolphin_cpu_core", "CPU Emulation Engine", "JIT",
        {{"Interpreter (Slowest)","Interpreter"},{"Cached Interpreter (Slow)","Cached Interpreter"},{"JIT Recompiler (Recommended)","JIT"}},
        "The CPU backend. JIT is required for full-speed gameplay; the interpreters are debug/accuracy fallbacks."));
    s.append(opt("Advanced", "CPU", "dolphin_mmu", "Enable MMU", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Emulate the memory management unit. Slower but required by a small set of games."));
    s.append(opt("Advanced", "CPU", "dolphin_pause_on_panic", "Pause on Panic", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pause emulation when Dolphin reports a non-fatal error."));
    s.append(opt("Advanced", "CPU", "dolphin_accurate_cpu_cache", "Enable Write-Back Cache (Slow)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Emulate the CPU's L1 cache. Slower but more accurate; needed for a handful of self-modifying-code games."));
    s.append(opt("Advanced", "Timing", "dolphin_correct_time_drift", "Correct Time Drift", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Compensate for accumulated frame-pacing drift over long sessions."));
    s.append(opt("Advanced", "Timing", "dolphin_rush_frame_presentation", "Rush Frame Presentation", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Aggressively present frames as soon as they're ready. Lower latency, more tearing without V-Sync."));
    s.append(opt("Advanced", "Timing", "dolphin_smooth_early_presentation", "Smooth Early Presentation", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Smooth pacing for frames that finish ahead of schedule."));
    s.append(opt("Advanced", "Clock Override", "dolphin_overclock_enable", "Enable CPU Clock Override", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Allow the multiplier below to scale the emulated CPU clock. Some games run smoother overclocked; others crash."));
    s.append(opt("Advanced", "Clock Override", "dolphin_overclock", "CPU Overclock Multiplier", "1",
        {{"1x (Native)","1"},{"2x (+100%)","2"},{"3x (+200%)","3"},{"4x (+300%)","4"}},
        "Multiplier on the emulated CPU clock when overclocking is enabled. 1x = native.", "dolphin_overclock_enable"));
    s.append(opt("Advanced", "VBI Override", "dolphin_vi_overclock_enable", "Enable VBI Frequency Override", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Scale the video-interface clock independently of the CPU. Affects refresh-rate timing for some games."));
    s.append(opt("Advanced", "VBI Override", "dolphin_vi_overclock", "VI Overclock Multiplier", "1",
        {{"1x (Native)","1"},{"2x","2"},{"3x","3"},{"4x","4"}},
        "Multiplier on the VI clock when VI overclocking is enabled.", "dolphin_vi_overclock_enable"));

    // ═══ GameCube ═══
    s.append(opt("GameCube", "IPL", "dolphin_skip_ipl", "Skip Main Menu (IPL)", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the GameCube boot animation and start the game directly. When off, requires IPL.bin in the BIOS folder."));
    s.append(opt("GameCube", "IPL", "dolphin_gc_language", "System Language", "0",
        {{"English","0"},{"German","1"},{"French","2"},{"Spanish","3"},{"Italian","4"},{"Dutch","5"}},
        "System language used by GameCube games that respect it."));
    s.append(opt("GameCube", "Devices", "dolphin_slot_a", "Slot A", "8",
        {{"Nothing","255"},{"Dummy","0"},{"Memory Card","1"},{"GCI Folder","8"},{"USB Gecko","7"},{"Advance Game Port","9"},{"Microphone","4"}},
        "Device in the GameCube's left memory-card / EXI slot."));
    s.append(opt("GameCube", "Devices", "dolphin_slot_b", "Slot B", "255",
        {{"Nothing","255"},{"Dummy","0"},{"Memory Card","1"},{"GCI Folder","8"},{"USB Gecko","7"},{"Advance Game Port","9"},{"Microphone","4"}},
        "Device in the GameCube's right memory-card / EXI slot."));
    s.append(opt("GameCube", "Devices", "dolphin_serial_port_1", "Serial Port 1 (SP1)", "255",
        {{"Nothing","255"},{"Dummy","0"},{"Broadband Adapter (TAP)","5"},{"Broadband Adapter (XLink Kai)","10"},{"Broadband Adapter (tapserver)","11"},{"Broadband Adapter (HLE)","12"},{"Modem Adapter (tapserver)","13"},{"Triforce AM-Baseboard","6"}},
        "Device on the GameCube's serial port — network adapters in compatible games."));

    // ═══ Wii ═══
    s.append(opt("Wii", "Misc", "dolphin_wii_keyboard", "Connect USB Keyboard", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Make a USB keyboard visible to Wii software."));
    s.append(opt("Wii", "Misc", "dolphin_enable_wiilink", "Enable WiiConnect24 (WiiLink)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Patch the Wii Shop / Channels to use community WiiLink servers. Off by default to avoid third-party network calls."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card", "Insert SD Card", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Make a virtual SD card visible to Wii software. Required for save imports, channel installs, and SD-using homebrew."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card_writes", "Allow Writes to SD Card", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "When off, the SD card is read-only — protects a shared image from accidental modification."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card_folder_sync", "Auto-Sync SD with Folder", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Mirror the SD card image from a host folder."));
    s.append(opt("Wii", "SD Card", "dolphin_wii_sd_card_size", "SD Card Size", "0",
        {{"Auto","0"},{"64 MiB","67108864"},{"128 MiB","134217728"},{"256 MiB","268435456"},{"512 MiB","536870912"},{"1 GiB","1073741824"},{"2 GiB","2147483648"},{"4 GiB (SDHC)","4294967296"},{"8 GiB (SDHC)","8589934592"},{"16 GiB (SDHC)","17179869184"},{"32 GiB (SDHC)","34359738368"}},
        "Capacity of the virtual SD card. Auto uses the image file as-is."));

    // ═══ Recommended (curated cross-category VIEW — re-references existing keys) ═══
    // Performance
    s.append(opt("Recommended", "Performance", "dolphin_cpu_thread", "Dual Core (Speed Hack)", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Run CPU and GPU emulation on separate threads. Big speed gain for most games."));
    s.append(opt("Recommended", "Performance", "dolphin_shader_compilation", "Shader Compilation", "Specialized",
        {{"Specialized (Default)","Specialized"},{"Exclusive Ubershaders","Exclusive Ubershaders"},{"Hybrid Ubershaders","Hybrid Ubershaders"},{"Skip Drawing","Skip Drawing"}},
        "Ubershader modes avoid shader-compile stutter at a GPU cost."));
    s.append(opt("Recommended", "Performance", "dolphin_wait_for_shaders", "Compile Shaders Before Starting", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Pre-compile the shader pipeline before launching. Slower start, smoother first minutes."));
    // Performance Hacks
    s.append(opt("Recommended", "Performance Hacks", "dolphin_store_efb_to_texture", "Store EFB Copies to Texture Only", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the slow EFB->RAM copy. Big speed boost; can break games that read EFB on the CPU."));
    s.append(opt("Recommended", "Performance Hacks", "dolphin_store_xfb_to_texture", "Store XFB Copies to Texture Only", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the slow XFB->RAM copy. Big speed boost; required off for games that decode the XFB on the CPU."));
    s.append(opt("Recommended", "Performance Hacks", "dolphin_skip_efb_access", "Skip EFB Access from CPU", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip CPU read-back of the EFB. Faster; disable for games that need accurate EFB access."));
    s.append(opt("Recommended", "Performance Hacks", "dolphin_texcache_accuracy", "Texture Cache Accuracy", "Default",
        {{"Safe (Slowest)","Safe"},{"Default","Default"},{"Fast","Fast"}},
        "Fast = best performance with glitch risk; Safe = full accuracy. Default is balanced."));
    // Visual Quality
    s.append(opt("Recommended", "Visual Quality", "dolphin_internal_resolution", "Internal Resolution", "1x",
        {{"Auto (Window Size)","Auto"},{"Native (1x)","1x"},{"2x","2x"},{"3x","3x"},{"4x","4x"},{"5x","5x"},{"6x (4K)","6x"},{"7x","7x"},{"8x","8x"}},
        "Render scale relative to native. The single biggest knob for visual fidelity."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_aspect_ratio", "Aspect Ratio", "Auto",
        {{"Auto","Auto"},{"Force 16:9","16:9"},{"Force 4:3","4:3"},{"Stretch to Window","Stretch"}},
        "Display aspect ratio. Auto matches the game."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_antialiasing", "Anti-Aliasing", "None",
        {{"None","None"},{"2x MSAA","2x MSAA"},{"4x MSAA","4x MSAA"},{"8x MSAA","8x MSAA"},{"2x SSAA","2x SSAA"},{"4x SSAA","4x SSAA"},{"8x SSAA","8x SSAA"}},
        "Smooths edges. SSAA is far more demanding than MSAA."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_texture_filtering", "Texture Filtering", "Default",
        {{"Default","Default"},{"1x Anisotropic","1x Anisotropic"},{"2x Anisotropic","2x Anisotropic"},{"4x Anisotropic","4x Anisotropic"},{"8x Anisotropic","8x Anisotropic"},{"16x Anisotropic","16x Anisotropic"},{"Force Nearest and 1x Anisotropic","Force Nearest and 1x Anisotropic"},{"Force Linear and 1x Anisotropic","Force Linear and 1x Anisotropic"},{"Force Linear and 2x Anisotropic","Force Linear and 2x Anisotropic"},{"Force Linear and 4x Anisotropic","Force Linear and 4x Anisotropic"},{"Force Linear and 8x Anisotropic","Force Linear and 8x Anisotropic"},{"Force Linear and 16x Anisotropic","Force Linear and 16x Anisotropic"}},
        "Sharpens distant textures and optionally forces a magnification filter."));
    s.append(opt("Recommended", "Visual Quality", "dolphin_widescreen_hack", "Widescreen Hack", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Force 4:3 games to render in widescreen. May produce artifacts."));
    // Audio
    s.append(opt("Recommended", "Audio", "dolphin_dsp_engine", "DSP Emulation Engine", "HLE",
        {{"HLE (Recommended)","HLE"},{"LLE Recompiler (Slow)","LLE Recompiler"},{"LLE Interpreter (Very Slow)","LLE Interpreter"}},
        "HLE is fast and compatible. Use LLE only when a game needs it."));
    s.append(opt("Recommended", "Audio", "dolphin_volume", "Volume", "100",
        {{"0%","0"},{"10%","10"},{"20%","20"},{"30%","30"},{"40%","40"},{"50%","50"},{"60%","60"},{"70%","70"},{"80%","80"},{"90%","90"},{"100%","100"}},
        "Master output volume."));
    // Convenience
    s.append(opt("Recommended", "Convenience", "dolphin_enable_cheats", "Enable Cheats", "disabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Process AR/Gecko cheat codes."));
    s.append(opt("Recommended", "Convenience", "dolphin_skip_ipl", "Skip GameCube Boot Animation", "enabled",
        {{"Enabled","enabled"},{"Disabled","disabled"}},
        "Skip the GC IPL boot sequence and start the game directly."));

    return s;
}

QVector<SettingsHubCard> DolphinLibretroAdapter::settingsHubCards() const {
    return {
        {QStringLiteral("\U0001F3A8"), "Graphics",
         "Resolution, AA, enhancements, hacks, OSD",
         "Graphics", 0, 0},
    };
}

PreviewSpec DolphinLibretroAdapter::previewSpec(const QString& category,
                                                const QString& subcategory) const {
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"dolphin_show_fps",     "showFps"},
            {"dolphin_show_speed",   "showSpeed"},
            {"dolphin_show_vps",     "showVps"},
            {"dolphin_show_ftimes",  "showFrameTimes"},
            {"dolphin_osd_messages", "showSettings"},
        }};
    }
    return {};
}

bool DolphinLibretroAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                          const QString& /*biosPath*/,
                                          const QString& savesPath) {
    QDir().mkpath(savesPath);
    QDir().mkpath(QFileInfo(optionsJsonPath()).absolutePath());

    // Seed a fresh controls.ini from each binding def's own defaultValue. The
    // base class seeds from a shared hardcoded slot->element map that doesn't
    // know GC L/R are analog triggers; seeding from defaultValue keeps the
    // fresh-file defaults identical to what Auto-Map writes. Never overwrite an
    // existing file — user remaps are preserved across launches.
    const QString iniPath = controlsIniPath();
    if (!QFileInfo::exists(iniPath)) {
        const QString section = controllerBindingsSection(/*port=*/1);
        IniFile ini;
        for (const auto& def : controllerBindingDefsForType({})) {
            if (!def.defaultValue.isEmpty())
                ini.setValue(section, def.key, def.defaultValue);
        }
        if (!ini.save(iniPath))
            qWarning() << "[DolphinLibretroAdapter] Failed to write default controls.ini to" << iniPath;
        else
            qInfo() << "[DolphinLibretroAdapter] Seeded controls.ini from binding defs at" << iniPath;
    }
    return true;
}
