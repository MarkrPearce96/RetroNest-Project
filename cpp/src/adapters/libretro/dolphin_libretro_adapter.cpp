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
