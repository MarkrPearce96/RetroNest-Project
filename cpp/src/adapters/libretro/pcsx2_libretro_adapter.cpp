#include "pcsx2_libretro_adapter.h"

#include "core/binding_def.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"

#include <QDir>

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

// SP6.5: GameSession::terminate writes "{serial}.resume" under
// emulators/pcsx2/ps2/savestates/. Look there. Base id is "pcsx2"
// (the manifest id used by Paths::emulatorDataDir on the save side
// at game_session.cpp:392); systemId is "ps2".
QString Pcsx2LibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    // Path overrides: search the override dir first if set, else fall
    // back to the default <emulator_data>/savestates. Must mirror the
    // write side in GameSession::terminate / libretroSlotPath, otherwise
    // a user with a SaveStates override would silently lose cold-resume.
    QString dir = PathOverridesStore::instance().read("pcsx2", "SaveStates");
    if (dir.isEmpty())
        dir = Paths::emulatorDataDir("pcsx2", "ps2") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    if (!entries.isEmpty())
        return d.absoluteFilePath(entries.first());
    return {};
}

// Path overrides: three user-overridable folders exposed in the Paths UI.
// "libretro" section is informational — ConfigService routes libretro adapters
// to PathOverridesStore, so only the key identifies the override within the
// "pcsx2" namespace. defaultSuffix values match what the runtime consumers
// expect:
//   memcards   — pcsx2-libretro/Settings.cpp uses save_dir + "/memcards"
//   savestates — per-emulator data dir + "/savestates"
//   textures   — pcsx2-libretro/Settings.cpp uses save_dir + "/textures"
// BIOS is intentionally not overridable per-emulator — see spec.
QVector<PathDef> Pcsx2LibretroAdapter::pathsDefs() const {
    return {
        { "Memory Cards", "libretro", "MemoryCards", "memcards",   PathBase::EmulatorData },
        { "Save States",  "libretro", "SaveStates",  "savestates", PathBase::EmulatorData },
        { "Textures",     "libretro", "Textures",    "textures",   PathBase::EmulatorData },
    };
}

// SP7b: libretro-option-backed rows for the per-emulator settings dialog.
// Pattern mirrors MgbaLibretroAdapter::settingsSchema (sibling adapter
// in the same directory). The three keys and their values exactly match
// pcsx2-libretro/CoreOptions.cpp's kDefinitions[] — OptionsStore::load
// reconciles host options.json against the core-declared values list and
// drops any value not on the list, so divergence here silently wipes user
// settings.
QVector<SettingDef> Pcsx2LibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    auto opt = [](const QString& category,
                  const QString& group,
                  const QString& key,
                  const QString& label,
                  const QString& def,
                  const QVector<QPair<QString,QString>>& valuesAndLabels,
                  const QString& tooltip,
                  const QString& dependsOn = {}) -> SettingDef {
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

    // SP7c Phase 4: Graphics-card rows carry a subcategory ("Display",
    // "Rendering", "Texture Replacement", "Post-Processing",
    // "On-Screen Display"). The dialog flips hasSubTabs=true for
    // category="Graphics" so GenericSettingsPage's sub-tab bar picks up
    // each distinct subcategory string. gopt() is opt() with category
    // hardcoded to "Graphics" and subcategory pushed into d.subcategory.
    // tools/check_schema_fidelity.py's HOST_BLOCK_RE accepts both opt()
    // and gopt() callsites (the parser pulls (key, default, values) from
    // the same positional layout in each).
    auto gopt = [](const QString& subcategory,
                   const QString& group,
                   const QString& key,
                   const QString& label,
                   const QString& def,
                   const QVector<QPair<QString,QString>>& valuesAndLabels,
                   const QString& tooltip,
                   const QString& dependsOn = {}) -> SettingDef {
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

    // Shared value lists. Hoisted ahead of the Recommended block (which
    // references both) so they're in scope for every use site below. The
    // schema-fidelity tool (tools/check_schema_fidelity.py) resolves the
    // identifier via HOST_VALUES_REF_RE, so referencing by identifier is
    // first-class supported.
    const QVector<QPair<QString,QString>> speedOptions = {
        {"2% [1 FPS (NTSC) / 1 FPS (PAL)]",       "0.02"},
        {"10% [6 FPS (NTSC) / 5 FPS (PAL)]",      "0.1"},
        {"25% [15 FPS (NTSC) / 12 FPS (PAL)]",    "0.25"},
        {"50% [30 FPS (NTSC) / 25 FPS (PAL)]",    "0.5"},
        {"75% [45 FPS (NTSC) / 37 FPS (PAL)]",    "0.75"},
        {"90% [54 FPS (NTSC) / 45 FPS (PAL)]",    "0.9"},
        {"100% [60 FPS (NTSC) / 50 FPS (PAL)]",   "1"},
        {"110% [66 FPS (NTSC) / 55 FPS (PAL)]",   "1.1"},
        {"120% [72 FPS (NTSC) / 60 FPS (PAL)]",   "1.2"},
        {"150% [90 FPS (NTSC) / 75 FPS (PAL)]",   "1.5"},
        {"175% [105 FPS (NTSC) / 87 FPS (PAL)]",  "1.75"},
        {"200% [120 FPS (NTSC) / 100 FPS (PAL)]", "2"},
        {"300% [180 FPS (NTSC) / 150 FPS (PAL)]", "3"},
        {"400% [240 FPS (NTSC) / 200 FPS (PAL)]", "4"},
        {"500% [300 FPS (NTSC) / 250 FPS (PAL)]", "5"},
        {"1000% [600 FPS (NTSC) / 500 FPS (PAL)]","10"},
        {"Unlimited", "0"},
    };
    const QVector<QPair<QString,QString>> volumeOptions = {
        {"0% (Muted)",       "0"},
        {"25%",              "25"},
        {"50%",              "50"},
        {"75%",              "75"},
        {"100% (default)",   "100"},
        {"125%",             "125"},
        {"150%",             "150"},
        {"175%",             "175"},
        {"200% (max)",       "200"},
    };

    // SP7c followup — Recommended card mirrors the standalone Pcsx2Adapter
    // "Recommended" page (cpp/src/adapters/pcsx2_adapter.cpp:43-185). Curated
    // view: each row references an existing pcsx2_* core option from Phases 1-4
    // so the user can tweak the most-common knobs without hunting through
    // sub-tabs. Edits route to the same backing core option as the per-tab
    // row, so the two views stay in lock-step automatically. Audio Backend
    // intentionally omitted — the libretro variant forces backend="Libretro"
    // (SP4 architectural decision); a configurable backend row would be
    // misleading.

    // ── Performance ──────────────────────────────────────────────────────
    s.append(opt(
        "Recommended", "Performance",
        "pcsx2_renderer", "GS Renderer", "auto",
        {{"Auto", "auto"},
         {"Metal", "metal"},
         {"Software", "software"},
         {"Null", "null"}},
        "PCSX2 graphics backend. Auto picks Metal on macOS. Software is "
        "CPU-only and much slower; useful for debugging rendering bugs "
        "or working around hardware-renderer regressions in specific games. "
        "Takes effect on next launch."));

    s.append(opt(
        "Recommended", "Performance",
        "pcsx2_mtvu", "Multi-Threaded VU1", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Run the VU1 microprogram on its own thread instead of the EE "
        "thread. Compatible with the vast majority of games and "
        "significantly reduces EE-thread saturation on Apple Silicon. "
        "Disable only if a specific game shows MTVU-related glitches. "
        "Takes effect on next launch."));

    s.append(opt(
        "Recommended", "Performance",
        "pcsx2_ee_cycle_rate", "EE Cycle Rate", "0",
        {{"50% (Underclock)",  "-3"},
         {"60% (Underclock)",  "-2"},
         {"75% (Underclock)",  "-1"},
         {"100% (Normal Speed)","0"},
         {"130% (Overclock)",  "1"},
         {"180% (Overclock)",  "2"},
         {"300% (Overclock)",  "3"}},
        "Underclocks or overclocks the emulated Emotion Engine CPU. "
        "Most games should stay at 100%. Takes effect on next launch."));

    // ── Visual Quality ───────────────────────────────────────────────────
    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_upscale_multiplier", "Internal Resolution", "1",
        {{"1x Native (PS2) (Default)",     "1"},
         {"2x Native (~720px/HD)",         "2"},
         {"3x Native (~1080px/FHD)",       "3"},
         {"4x Native (~1440px/QHD)",       "4"},
         {"5x Native (~1800px/QHD+)",      "5"},
         {"6x Native (~2160px/4K UHD)",    "6"},
         {"7x Native (~2520px)",           "7"},
         {"8x Native (~2880px/5K UHD)",    "8"},
         {"9x Native (~3240px)",           "9"},
         {"10x Native (~3600px/6K UHD)",  "10"},
         {"11x Native (~3960px)",         "11"},
         {"12x Native (~4320px/8K UHD)",  "12"}},
        "Sets the internal rendering resolution. Higher values produce "
        "sharper visuals at the cost of GPU performance."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_aspect_ratio", "Aspect Ratio", "4:3",
        {{"Auto (4:3 Interlaced / 3:2 Progressive)", "Auto 4:3/3:2"},
         {"4:3 (Standard)",                          "4:3"},
         {"16:9 (Widescreen)",                       "16:9"},
         {"10:7 (Full/Native)",                      "10:7"},
         {"Stretch",                                 "Stretch"}},
        "Controls the aspect ratio of the emulated display. Auto picks "
        "4:3 for interlaced games and 3:2 for progressive games. 16:9 "
        "stretches the image for widescreen TVs; Stretch fills the whole "
        "window."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_enable_widescreen_patches", "Apply Widescreen Patches", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Automatically applies community widescreen patches to supported "
        "games. Reshapes the rendering to true 16:9 instead of stretching "
        "the 4:3 picture."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_accurate_blending_unit", "Blending Accuracy", "1",
        {{"Minimum",          "0"},
         {"Basic (Default)",  "1"},
         {"Medium",           "2"},
         {"High",             "3"},
         {"Full",             "4"},
         {"Maximum",          "5"}},
        "Controls how accurately PS2 blending operations are emulated. "
        "Higher levels improve compatibility with heavy effects at a "
        "performance cost."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_max_anisotropy", "Anisotropic Filtering", "0",
        {{"Off (Default)",  "0"},
         {"2x",             "2"},
         {"4x",             "4"},
         {"8x",             "8"},
         {"16x",           "16"}},
        "Improves texture clarity at oblique viewing angles. Low cost on "
        "modern GPUs and generally safe to raise."));

    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_filter", "Texture Filtering", "2",
        {{"Nearest",                            "0"},
         {"Bilinear (Forced)",                  "1"},
         {"Bilinear (PS2) (Default)",           "2"},
         {"Bilinear (Forced excluding sprite)", "3"}},
        "Controls how textures are sampled when rendered. Bilinear (PS2) "
        "matches the original hardware behavior; Forced options ignore "
        "the game's preference."));

    // ── Frame Pacing ─────────────────────────────────────────────────────
    s.append(opt(
        "Recommended", "Frame Pacing",
        "pcsx2_vsync", "Vertical Sync (VSync)", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Synchronize frame submission with the host display's vblank. "
        "May be cosmetic in libretro mode. Takes effect on next launch."));

    s.append(opt(
        "Recommended", "Frame Pacing",
        "pcsx2_sync_to_host_rr", "Sync to Host Refresh Rate", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Adjust emulation speed slightly to align with the host display's "
        "refresh rate. May be cosmetic in libretro mode. Takes effect on "
        "next launch."));

    // ── Audio ────────────────────────────────────────────────────────────
    s.append(opt(
        "Recommended", "Audio",
        "pcsx2_audio_volume", "Volume", "100",
        volumeOptions,
        "Normal-play audio volume. 100% is the PS2's native output level. "
        "Values above 100% boost the signal digitally (may clip on loud "
        "passages). Takes effect on next launch."));

    // ── Convenience ──────────────────────────────────────────────────────
    s.append(opt(
        "Recommended", "Convenience",
        "pcsx2_normal_speed", "Normal Speed", "1",
        speedOptions,
        "Target emulation speed during normal gameplay (relative to PS2's "
        "native rate). 100% is real-time. Takes effect on next launch."));

    s.append(opt(
        "Recommended", "Convenience",
        "pcsx2_fast_boot", "Fast Boot", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Skip the PS2 BIOS Sony intro and region-check screen on launch. "
        "Disable if you want to see the BIOS screen (e.g. to verify your "
        "BIOS region or to use the BIOS browser). Takes effect on next launch."));

    s.append(opt(
        "Recommended", "Convenience",
        "pcsx2_cheats", "Enable Cheats", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Load pnach cheat files on game launch. Takes effect on next launch."));

    // SP7c Phase 1 — Speed Control (sub-group A of the Emulation card).
    // speedOptions hoisted to the top of this function (shared with the
    // Recommended-card mirror); values list mirrors the standalone PCSX2
    // dialog exactly (RetroNest's cpp/src/adapters/pcsx2_adapter.cpp:200-218).
    s.append(opt(
        "Emulation", "Speed Control",
        "pcsx2_normal_speed", "Normal Speed", "1",
        speedOptions,
        "Target emulation speed during normal gameplay (relative to PS2's "
        "native rate). 100% is real-time. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "Speed Control",
        "pcsx2_fast_forward_speed", "Fast-Forward Speed", "2",
        speedOptions,
        "Target speed when fast-forward is engaged. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "Speed Control",
        "pcsx2_slow_motion_speed", "Slow-Motion Speed", "0.5",
        speedOptions,
        "Target speed when slow-motion is engaged. Takes effect on next launch."));

    // SP7c Phase 1 — System Settings (sub-group B of the Emulation card).
    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_ee_cycle_rate", "EE Cycle Rate", "0",
        {{"50% (Underclock)",  "-3"},
         {"60% (Underclock)",  "-2"},
         {"75% (Underclock)",  "-1"},
         {"100% (Normal Speed)","0"},
         {"130% (Overclock)",  "1"},
         {"180% (Overclock)",  "2"},
         {"300% (Overclock)",  "3"}},
        "Underclocks or overclocks the emulated Emotion Engine CPU. "
        "Most games should stay at 100%. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_ee_cycle_skip", "EE Cycle Skipping", "0",
        {{"Disabled",            "0"},
         {"Mild Underclock",     "1"},
         {"Moderate Underclock", "2"},
         {"Maximum Underclock",  "3"}},
        "Makes the EE skip cycles. Stronger underclock than EE Cycle Rate; "
        "can recover frame-rate in slow scenes at the cost of visible "
        "glitches. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_mtvu", "Multi-Threaded VU1", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Run the VU1 microprogram on its own thread instead of the EE "
        "thread. Compatible with the vast majority of games and "
        "significantly reduces EE-thread saturation on Apple Silicon. "
        "Disable only if a specific game shows MTVU-related glitches. "
        "Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_thread_pinning", "Thread Pinning", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Pin emulation threads to specific CPU cores. Can reduce stutter "
        "on heterogeneous-core CPUs. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_cheats", "Enable Cheats", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Load pnach cheat files on game launch. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_host_fs", "Enable Host Filesystem", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Allow the emulated PS2 to read host files. Homebrew-only feature; "
        "retail games never use it. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_cdvd_precache", "CDVD Precache", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Load the entire disc image into RAM before booting. Eliminates "
        "disc-read stutter at the cost of memory and a slower initial "
        "boot. Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_fast_boot", "Fast Boot", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Skip the PS2 BIOS Sony intro and region-check screen on launch. "
        "Disable if you want to see the BIOS screen (e.g. to verify your "
        "BIOS region or to use the BIOS browser). Takes effect on next launch."));

    s.append(opt(
        "Emulation", "System Settings",
        "pcsx2_fast_boot_ff", "Fast-Forward Through BIOS", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "When Fast Boot is enabled, also fast-forward the brief BIOS boot "
        "animation. No effect when Fast Boot is disabled. Takes effect on "
        "next launch.",
        // dependsOn resolves to the Emulation > System Settings
        // pcsx2_fast_boot row (Phase 5); the Recommended-card copy is also a
        // valid master. Same-card resolution is more robust against future
        // cross-category-dependsOn changes — see memory cross_category_
        // dependson_limitation.
        "pcsx2_fast_boot"));

    // SP7c Phase 1 — Frame Pacing / Latency Control (sub-group C).
    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_vsync_queue_size", "Maximum Frame Latency", "2",
        {{"Optimal (Frame Pacing)", "0"},
         {"1 frame",                "1"},
         {"2 frames",               "2"},
         {"3 frames",               "3"}},
        "Frames the GS can queue before the EE must wait. Lower values "
        "reduce input latency at the cost of pacing smoothness. Takes "
        "effect on next launch."));

    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_sync_to_host_rr", "Sync to Host Refresh Rate", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Adjust emulation speed slightly to align with the host display's "
        "refresh rate. May be cosmetic in libretro mode. Takes effect on "
        "next launch."));

    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_vsync", "Vertical Sync (VSync)", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Synchronize frame submission with the host display's vblank. "
        "May be cosmetic in libretro mode. Takes effect on next launch."));

    // pcsx2_use_vsync_timing gates on the compound (VSync && SyncToHostRR);
    // SettingDef.dependsOn accepts "A && B" multi-master expressions.
    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_use_vsync_timing", "Use Host VSync Timing", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Drive emulation timing from host vsync instead of the emulated "
        "console's refresh. Only takes effect when both VSync and Sync "
        "to Host Refresh Rate are enabled. Takes effect on next launch.",
        "pcsx2_vsync && pcsx2_sync_to_host_rr"));

    s.append(opt(
        "Emulation", "Frame Pacing / Latency Control",
        "pcsx2_skip_duplicate_frames", "Skip Presenting Duplicate Frames", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Don't re-present a frame if the GS hasn't produced new output. "
        "Mostly cosmetic in libretro mode. Takes effect on next launch."));

    // Audio card.
    //
    // Only 2 of upstream PCSX2's 8 Audio > Configuration rows are
    // user-tweakable in this architecture, because audio routes through
    // LibretroAudioStream (libretro audio_batch_cb is the only audio path;
    // Cubeb/SDL/etc. are bypassed):
    //   • Backend — FORCED to "Libretro" in pcsx2-libretro/Settings.cpp
    //     (LibretroAudioStream is the only audio path). Skipped.
    //   • DriverName / DeviceName / OutputLatencyMS / OutputLatencyMinimal
    //     — owned by RetroNest's host SDL audio sink, which sits upstream
    //     of the libretro core. Skipped (host-side concern).
    //   • ExpansionMode — FORCED to "Disabled" in pcsx2-libretro/Settings.cpp
    //     because the libretro audio_batch_cb is stereo-only. Skipped.
    //
    // The two libretro-applicable rows (SyncMode, BufferMS) follow, plus
    // a Controls sub-group (Volume, Fast-Forward Volume, Mute).
    //
    // Value strings MUST match the core's CoreOptionsAudio.cpp byte-for-byte
    // (and the SyncMode strings must match PCSX2's ParseSyncMode). The
    // check_schema_fidelity.py target verifies this mechanically.
    s.append(opt(
        "Audio", "Configuration",
        "pcsx2_audio_sync_mode", "Audio Sync Mode", "TimeStretch",
        {{"Disabled (Noisy)",         "Disabled"},
         {"TimeStretch (Recommended)","TimeStretch"}},
        "How emulated audio is paced against host audio. TimeStretch "
        "resamples audio (via SoundTouch) so pitch stays correct when "
        "emulation speed differs from 100%. Disabled passes raw samples "
        "through — fastest but produces audible artefacts during any "
        "speed deviation. Takes effect on next launch."));

    s.append(opt(
        "Audio", "Configuration",
        "pcsx2_audio_buffer_ms", "Audio Buffer Size", "50",
        {{"10 ms (lowest latency)", "10"},
         {"20 ms",                  "20"},
         {"30 ms",                  "30"},
         {"50 ms (default)",        "50"},
         {"75 ms",                  "75"},
         {"100 ms",                 "100"},
         {"150 ms",                 "150"},
         {"200 ms",                 "200"}},
        "Ring-buffer size for emulated audio in milliseconds. Smaller "
        "values reduce audio latency at the cost of higher CPU pressure "
        "and a greater chance of underruns. Takes effect on next launch."));

    // Volume + Fast-Forward Volume share the same 9-stop list. volumeOptions
    // hoisted to the top of this function (shared with the Recommended-card
    // mirror); check_schema_fidelity.py resolves the identifier via
    // HOST_VALUES_REF_RE.
    s.append(opt(
        "Audio", "Controls",
        "pcsx2_audio_volume", "Volume", "100",
        volumeOptions,
        "Normal-play audio volume. 100% is the PS2's native output level. "
        "Values above 100% boost the signal digitally (may clip on loud "
        "passages). Takes effect on next launch."));

    s.append(opt(
        "Audio", "Controls",
        "pcsx2_audio_ff_volume", "Fast-Forward Volume", "100",
        volumeOptions,
        "Volume during fast-forward. Independent from normal-play volume — "
        "useful for muting audio entirely during fast-forward. Takes "
        "effect on next launch."));

    s.append(opt(
        "Audio", "Controls",
        "pcsx2_audio_muted", "Mute Audio", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Mute all PS2 audio output. RetroNest's UI sounds and other "
        "non-PS2 audio sources are unaffected. Takes effect on next launch."));

    // Memory Cards card.
    //
    // 5 rows under category="Memory Cards" mirroring the standalone PCSX2
    // dialog at cpp/src/adapters/pcsx2_adapter.cpp:986-1029, with these
    // intentional drops:
    //   • Slot1_Filename / Slot2_Filename — standalone exposes these as
    //     free-form text inputs ("Mcd001.ps2" / "Mcd002.ps2" by default).
    //     libretro core options are Combo-only (no free-form string control
    //     type), so the filenames are hardcoded to the same defaults in
    //     pcsx2-libretro/Settings.cpp. User-tweakable filename support
    //     would require either a new control type in the core-options ABI
    //     or an out-of-band RetroNest-side override — deferred.
    //
    // Slot2_Enable defaults to "enabled" matching standalone (was "disabled"
    // pre-Phase-3, a SP6 single-slot convention). Behavioral change is
    // mostly invisible — PCSX2 only auto-creates Mcd002.ps2 on first WRITE
    // to Slot 2, so users that don't actively use Slot 2 see no change.
    //
    // Value strings MUST match the core's CoreOptionsMemoryCards.cpp
    // byte-for-byte. The check_schema_fidelity.py target verifies this
    // mechanically.
    s.append(opt(
        "Memory Cards", "Memory Card Slots",
        "pcsx2_mc_slot1_enable", "Memory Card Slot 1", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Inserts a virtual memory card into Slot 1. Stored as Mcd001.ps2 "
        "under the per-game memcards folder. Disabling prevents games "
        "from saving/loading via Slot 1. Takes effect on next launch."));

    s.append(opt(
        "Memory Cards", "Memory Card Slots",
        "pcsx2_mc_slot2_enable", "Memory Card Slot 2", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Inserts a virtual memory card into Slot 2. Stored as Mcd002.ps2 "
        "under the per-game memcards folder. PCSX2 only auto-creates the "
        "file the first time a game writes to Slot 2. Takes effect on "
        "next launch."));

    s.append(opt(
        "Memory Cards", "Multitap",
        "pcsx2_mc_multitap1_slot2", "Multitap 1 - Slot 2", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables the second memory-card slot of Multitap 1. Only useful "
        "when a game supports Multitap 1 and you need additional save "
        "slots for extra players. Takes effect on next launch."));

    s.append(opt(
        "Memory Cards", "Multitap",
        "pcsx2_mc_multitap1_slot3", "Multitap 1 - Slot 3", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables the third memory-card slot of Multitap 1. Takes effect "
        "on next launch."));

    s.append(opt(
        "Memory Cards", "Multitap",
        "pcsx2_mc_multitap1_slot4", "Multitap 1 - Slot 4", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables the fourth memory-card slot of Multitap 1. Takes effect "
        "on next launch."));

    // ═══════════════════════════════════════════════════════════════════
    // SP7c Phase 4 — Graphics card
    // ═══════════════════════════════════════════════════════════════════
    //
    // Mirrors the standalone PCSX2 dialog's Graphics widget sub-tab
    // structure. Subcategory drives the sub-tab grouping in
    // GenericSettingsPage once the dialog's hasSubTabs flag is set
    // (Phase 4 Task 7 flips it for category="Graphics").
    //
    // Renderer is cross-listed under category="Recommended" (Phase 0) AND
    // category="Graphics" subcategory="Display" (Phase 5). The libretro
    // variant's Renderer enum is libretro-side (Auto/Metal/Software/Null),
    // not standalone-side (Auto/OpenGL/Vulkan/Metal/Software). Both rows
    // point at the same backing core option pcsx2_renderer; edits in
    // either view route to the same storage. Keep their value lists +
    // tooltips in sync — Recommended source-of-truth row lives at the top
    // of settingsSchema().

    // ── Graphics > Display (17 knobs) — Phase 4 Task 2 + Phase 5 ──────

    // Mirrors the Recommended → Performance row; keep value list and
    // tooltip in sync with the source-of-truth row above.
    s.append(gopt(
        "Display", "Display",
        "pcsx2_renderer", "Renderer", "auto",
        {{"Auto", "auto"},
         {"Metal", "metal"},
         {"Software", "software"},
         {"Null", "null"}},
        "PCSX2 graphics backend. Auto picks Metal on macOS. Software is "
        "CPU-only and much slower; useful for debugging rendering bugs "
        "or working around hardware-renderer regressions in specific games. "
        "Takes effect on next launch."));

    // Aspect-ratio enum combo: values match the INI string verbatim.
    s.append(gopt(
        "Display", "Display",
        "pcsx2_aspect_ratio", "Aspect Ratio", "4:3",
        {{"Auto (4:3 Interlaced / 3:2 Progressive)", "Auto 4:3/3:2"},
         {"4:3 (Standard)",                          "4:3"},
         {"16:9 (Widescreen)",                       "16:9"},
         {"10:7 (Full/Native)",                      "10:7"},
         {"Stretch",                                 "Stretch"}},
        "Controls the aspect ratio of the emulated display. Auto picks "
        "4:3 for interlaced games and 3:2 for progressive games. 16:9 "
        "stretches the image for widescreen TVs; Stretch fills the whole "
        "window."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_fmv_aspect_ratio", "FMV Aspect Ratio Override", "Off",
        {{"Off (Default)",                              "Off"},
         {"Auto (4:3 Interlaced / 3:2 Progressive)",    "Auto 4:3/3:2"},
         {"4:3 (Standard)",                             "4:3"},
         {"16:9 (Widescreen)",                          "16:9"},
         {"10:7 (Full/Native)",                         "10:7"}},
        "Overrides the aspect ratio only while full-motion video (FMV) "
        "is playing. Useful for games with widescreen cutscenes inside "
        "a 4:3 main game."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_deinterlace_mode", "Deinterlacing", "0",
        {{"Automatic (Default)", "0"},
         {"Off",                 "1"},
         {"Weave (Top)",         "2"},
         {"Weave (Bottom)",      "3"},
         {"Bob (Top)",           "4"},
         {"Bob (Bottom)",        "5"},
         {"Blend (Top)",         "6"},
         {"Blend (Bottom)",      "7"},
         {"Adaptive (Top)",      "8"},
         {"Adaptive (Bottom)",   "9"}},
        "Selects how interlaced frames are combined for progressive "
        "display. Automatic picks the best option per game; Weave "
        "preserves detail; Bob and Blend smooth motion at the cost of "
        "vertical resolution."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_linear_present_mode", "Bilinear Filtering", "1",
        {{"None",                         "0"},
         {"Bilinear (Smooth) (Default)",  "1"},
         {"Bilinear (Sharp)",             "2"}},
        "Applies a bilinear filter when scaling the final image to the "
        "window. Smooth is the standard option; Sharp uses a pixel-art-"
        "friendly variant that keeps edges crisp."));

    // ── Stretch + crop (5 int-as-Combo knobs) ──
    // Standalone is a 1%-step Qt slider; libretro is Combo-only so we
    // expose enumerated stops clustered near the default.
    s.append(gopt(
        "Display", "Display",
        "pcsx2_stretch_y", "Vertical Stretch", "100",
        {{"50%",            "50"},
         {"75%",            "75"},
         {"90%",            "90"},
         {"100% (Default)", "100"},
         {"110%",           "110"},
         {"125%",           "125"},
         {"150%",           "150"},
         {"200%",           "200"},
         {"300%",           "300"}},
        "Multiplies the display height after aspect-ratio fitting. "
        "Values above 100% make the image taller than its letterbox; "
        "values below leave extra vertical space. Standalone PCSX2 "
        "exposes a 10-300% slider; libretro offers enumerated stops."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_crop_left", "Crop - Left", "0",
        {{"0 px (Default)", "0"},
         {"1 px",           "1"},
         {"2 px",           "2"},
         {"3 px",           "3"},
         {"5 px",           "5"},
         {"10 px",          "10"},
         {"15 px",          "15"},
         {"20 px",          "20"},
         {"30 px",          "30"},
         {"50 px",          "50"},
         {"100 px",         "100"}},
        "Trims pixels from the left edge of the source image before "
        "it's fit to the display window. Useful for games with garbage "
        "pixels at the border."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_crop_top", "Crop - Top", "0",
        {{"0 px (Default)", "0"},
         {"1 px",           "1"},
         {"2 px",           "2"},
         {"3 px",           "3"},
         {"5 px",           "5"},
         {"10 px",          "10"},
         {"15 px",          "15"},
         {"20 px",          "20"},
         {"30 px",          "30"},
         {"50 px",          "50"},
         {"100 px",         "100"}},
        "Trims pixels from the top edge of the source image before "
        "it's fit to the display window."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_crop_right", "Crop - Right", "0",
        {{"0 px (Default)", "0"},
         {"1 px",           "1"},
         {"2 px",           "2"},
         {"3 px",           "3"},
         {"5 px",           "5"},
         {"10 px",          "10"},
         {"15 px",          "15"},
         {"20 px",          "20"},
         {"30 px",          "30"},
         {"50 px",          "50"},
         {"100 px",         "100"}},
        "Trims pixels from the right edge of the source image before "
        "it's fit to the display window."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_crop_bottom", "Crop - Bottom", "0",
        {{"0 px (Default)", "0"},
         {"1 px",           "1"},
         {"2 px",           "2"},
         {"3 px",           "3"},
         {"5 px",           "5"},
         {"10 px",          "10"},
         {"15 px",          "15"},
         {"20 px",          "20"},
         {"30 px",          "30"},
         {"50 px",          "50"},
         {"100 px",         "100"}},
        "Trims pixels from the bottom edge of the source image before "
        "it's fit to the display window."));

    // ── Display bools (7) ──
    s.append(gopt(
        "Display", "Display",
        "pcsx2_enable_widescreen_patches", "Apply Widescreen Patches", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Automatically applies community widescreen patches to supported "
        "games. Reshapes the rendering to true 16:9 instead of stretching "
        "the 4:3 picture."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_enable_no_interlacing_patches", "Apply No-Interlacing Patches", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Automatically applies community no-interlacing patches to "
        "supported games. Removes flicker in games that render in "
        "interlaced mode."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_pcrtc_antiblur", "Anti-Blur", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables internal anti-blur hacks that remove the PS2's GS smear "
        "on commonly-affected games. Safe to leave on."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_integer_scaling", "Integer Scaling", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Snaps the rendered image to an integer multiple of the source "
        "pixel size. Produces crisp pixel-art scaling at the cost of "
        "leaving letterbox bars."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_pcrtc_offsets", "Screen Offsets", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Enables PCRTC offsets so the screen is positioned exactly where "
        "the game requests. Fixes games that deliberately offset the "
        "viewport."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_disable_interlace_offset", "Disable Interlace Offset", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Disables the half-pixel interlace offset which can reduce "
        "jitter on some games that render at half vertical resolution."));

    s.append(gopt(
        "Display", "Display",
        "pcsx2_pcrtc_overscan", "Show Overscan", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Shows the overscan area of the display that would normally be "
        "hidden by a CRT bezel. Exposes any garbage the game draws "
        "outside the safe area."));

    // SP7c Phase 4 Task 3 — Rendering sub-tab (7 knobs).
    //
    // Value lists / defaults / labels mirror standalone PCSX2's
    // pcsx2_adapter.cpp Graphics/Rendering rows verbatim (lines
    // 451-503). check_schema_fidelity.py verifies byte-for-byte match
    // against the core's CoreOptionsGraphics.cpp.
    //
    // filter row's dependsOn uses the libretro-key form
    // (pcsx2_tri_filter!=2 && pcsx2_tri_filter!=3) — standalone stores
    // INI keys in dependsOn but the libretro adapter stores libretro
    // option keys (see pcsx2_libretro_adapter.cpp:302 for the
    // VSync && SyncToHostRR precedent). setting_dependency.h's
    // '!=' + '&&' grammar handles this.
    s.append(gopt(
        "Rendering", "Rendering",
        "pcsx2_upscale_multiplier", "Internal Resolution", "1",
        {{"1x Native (PS2) (Default)",     "1"},
         {"2x Native (~720px/HD)",         "2"},
         {"3x Native (~1080px/FHD)",       "3"},
         {"4x Native (~1440px/QHD)",       "4"},
         {"5x Native (~1800px/QHD+)",      "5"},
         {"6x Native (~2160px/4K UHD)",    "6"},
         {"7x Native (~2520px)",           "7"},
         {"8x Native (~2880px/5K UHD)",    "8"},
         {"9x Native (~3240px)",           "9"},
         {"10x Native (~3600px/6K UHD)",  "10"},
         {"11x Native (~3960px)",         "11"},
         {"12x Native (~4320px/8K UHD)",  "12"}},
        "Sets the internal rendering resolution. Higher values produce "
        "sharper visuals at the cost of GPU performance."));

    s.append(gopt(
        "Rendering", "Rendering",
        "pcsx2_filter", "Texture Filtering", "2",
        {{"Nearest",                            "0"},
         {"Bilinear (Forced)",                  "1"},
         {"Bilinear (PS2) (Default)",           "2"},
         {"Bilinear (Forced excluding sprite)", "3"}},
        "Controls how textures are sampled when rendered. Bilinear (PS2) "
        "matches the original hardware behavior; Forced options ignore "
        "the game's preference.",
        "pcsx2_tri_filter!=2 && pcsx2_tri_filter!=3"));

    s.append(gopt(
        "Rendering", "Rendering",
        "pcsx2_tri_filter", "Trilinear Filtering", "-1",
        {{"Auto (Default)",     "-1"},
         {"Off",                 "0"},
         {"Trilinear (PS2)",     "1"},
         {"Trilinear (Forced)",  "2"}},
        "Enables trilinear filtering for smoother transitions between "
        "mipmap levels. Auto leaves this decision to each game."));

    s.append(gopt(
        "Rendering", "Rendering",
        "pcsx2_max_anisotropy", "Anisotropic Filtering", "0",
        {{"Off (Default)",  "0"},
         {"2x",             "2"},
         {"4x",             "4"},
         {"8x",             "8"},
         {"16x",           "16"}},
        "Improves texture clarity at oblique viewing angles. Low cost on "
        "modern GPUs and generally safe to raise."));

    s.append(gopt(
        "Rendering", "Rendering",
        "pcsx2_dithering_ps2", "Dithering", "2",
        {{"Off",                 "0"},
         {"Scaled",              "1"},
         {"Unscaled (Default)",  "2"},
         {"Force 32bit",         "3"}},
        "Controls how PS2 dithering patterns are applied to upscaled "
        "rendering. Unscaled matches the original appearance."));

    s.append(gopt(
        "Rendering", "Rendering",
        "pcsx2_accurate_blending_unit", "Blending Accuracy", "1",
        {{"Minimum",          "0"},
         {"Basic (Default)",  "1"},
         {"Medium",           "2"},
         {"High",             "3"},
         {"Full",             "4"},
         {"Maximum",          "5"}},
        "Controls how accurately PS2 blending operations are emulated. "
        "Higher levels improve compatibility with heavy effects at a "
        "performance cost."));

    s.append(gopt(
        "Rendering", "Hardware Rendering Options",
        "pcsx2_hw_mipmap", "Hardware Mipmapping", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Emulates PS2 mipmapping when the hardware renderer is active. "
        "Fixes texture aliasing at distance in supported games."));

    // ── Graphics > Texture Replacement (Phase 4 Task 4) ──────────────────
    //
    // 6 bools mirroring standalone's GraphicsTextureReplacementSettingsTab.
    //
    // Renderer-gate dropped (Option B from the prep doc, after Option A
    // failed live-smoke): standalone gates every row on Renderer!=13
    // (Software). We initially translated that to
    // `pcsx2_renderer!=software` on the host side, but
    // GenericSettingsPage::refreshDependencies() builds masterValues
    // from `findChildren<SettingsComboRow *>()` of the current page only
    // — pcsx2_renderer lives on the Recommended page, so its value is
    // not visible to dependency evaluation here. The result was the
    // expression `"" != "software"` evaluating true and the row staying
    // editable in Software mode. Cross-page master propagation is a
    // dialog-level architectural change deferred as an SP7c followup.
    //
    // Master-bool chains within this page still work normally — both
    // pcsx2_load_texture_replacements and pcsx2_dump_replaceable_textures
    // are right here, so the four downstream rows grey correctly.
    //
    // UX impact of dropping the renderer gate: in Software mode the 6
    // rows stay editable but silently inert (PCSX2 ignores
    // LoadTextureReplacements etc. when GSConfig.UseHardwareRenderer()
    // is false). A minor wart for a niche scenario (user explicitly
    // switched to Software AND wants to twiddle texture-replacement
    // knobs).
    //
    // The texture-search-directory picker is dropped — RetroNest manages
    // EmuFolders::Textures from SP1 (texture dumps land per-game-serial
    // natively via Path::Combine(EmuFolders::Textures, s_current_serial)).
    s.append(gopt(
        "Texture Replacement", "Options",
        "pcsx2_load_texture_replacements", "Load Textures", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Loads replacement textures from the texture-replacement folder "
        "(per-game subdirectory keyed by disc serial). Only effective "
        "with a hardware renderer."));

    s.append(gopt(
        "Texture Replacement", "Options",
        "pcsx2_dump_replaceable_textures", "Dump Textures", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Dumps the game's textures to disk so they can be edited and "
        "loaded back as replacements. Writes to the per-game texture-"
        "replacement subdirectory. Only effective with a hardware "
        "renderer."));

    s.append(gopt(
        "Texture Replacement", "Options",
        "pcsx2_load_texture_replacements_async", "Asynchronous Texture Loading", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Loads replacement textures on a background thread to avoid "
        "stutter at first sight of each texture. Disable only for "
        "deterministic capture or debugging.",
        "pcsx2_load_texture_replacements"));

    s.append(gopt(
        "Texture Replacement", "Options",
        "pcsx2_precache_texture_replacements", "Precache Replacements", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Loads every replacement texture for the current game at boot "
        "instead of on-demand. Uses more memory but eliminates load "
        "stutter mid-game.",
        "pcsx2_load_texture_replacements"));

    s.append(gopt(
        "Texture Replacement", "Options",
        "pcsx2_dump_replaceable_mipmaps", "Dump Mipmaps", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "When dumping textures, also writes each mipmap level. Useful "
        "for replacing distance LODs explicitly. Only meaningful with "
        "Dump Textures enabled.",
        "pcsx2_dump_replaceable_textures"));

    s.append(gopt(
        "Texture Replacement", "Options",
        "pcsx2_dump_textures_with_fmv_active", "Dump Textures During FMV", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "Includes textures used during full-motion video in dumps. Off "
        "by default because FMV frames produce a large volume of one-"
        "off textures.",
        "pcsx2_dump_replaceable_textures"));

    // ── Graphics > Post-Processing (Phase 4 Task 5) ──────────────────────
    //
    // 9 knobs mirroring standalone's GraphicsPostProcessingSettingsTab.
    // Two groups: "Sharpening/Anti-Aliasing" (CAS×2 + FXAA) and
    // "Filters" (TV Shader, ShadeBoost master + 4 ShadeBoost sliders).
    // INI section is [EmuCore/GS] for all 9 (handled core-side in
    // CoreOptionsGraphics::ApplyDefaults).
    //
    // 5 standalone-side int sliders (CAS Sharpness + 4 ShadeBoost
    // rows) become Combo with stops 0/25/50/75/100 because libretro
    // core options v2 is Combo-only. Default 50 hits a stop and is the
    // neutral midpoint of PCSX2's shader formula value/50 (verified
    // against pcsx2/Config.h:741-744,894).
    //
    // dependsOn: pcsx2_cas_sharpness gates on pcsx2_cas_mode!=0
    // (value-equality form, Task 3 precedent: pcsx2_tri_filter!=2).
    // The 4 ShadeBoost sliders gate on pcsx2_shade_boost (master-bool
    // bare-key form, Task 4 precedent). All 5 dependsOn keys live
    // within the Graphics card — findChildren resolution works
    // correctly. Cross-category limitation does not apply here.

    s.append(gopt(
        "Post-Processing", "Sharpening/Anti-Aliasing",
        "pcsx2_cas_mode", "Contrast Adaptive Sharpening (CAS)", "0",
        {{"Disabled (Default)", "0"},
         {"Sharpen Only (Internal Resolution)", "1"},
         {"Sharpen and Resize (Display Resolution)", "2"}},
        "AMD's Contrast Adaptive Sharpening pass on the final image. "
        "Sharpen Only sharpens at the internal render resolution; "
        "Sharpen and Resize sharpens at the display resolution."));

    s.append(gopt(
        "Post-Processing", "Sharpening/Anti-Aliasing",
        "pcsx2_cas_sharpness", "CAS Sharpness", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Strength of the CAS sharpening pass. Higher values produce a "
        "sharper image with more visible noise. Standalone PCSX2 exposes "
        "a 1-100% slider; libretro offers enumerated stops.",
        "pcsx2_cas_mode!=0"));

    s.append(gopt(
        "Post-Processing", "Sharpening/Anti-Aliasing",
        "pcsx2_fxaa", "FXAA", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Fast Approximate Anti-Aliasing. A single-pass shader that "
        "softens jagged edges with low GPU cost."));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_tv_shader", "TV Shader", "0",
        {{"None (Default)", "0"},
         {"Scanline Filter", "1"},
         {"Diagonal Filter", "2"},
         {"Triangular Filter", "3"},
         {"Wave Filter", "4"},
         {"Lottes CRT", "5"},
         {"4xRGSS Downsampling", "6"},
         {"NxAGSS Downsampling", "7"}},
        "Applies a CRT-style filter to the final output for an authentic "
        "retro look. None disables the filter."));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost", "Shade Boost", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Master toggle for manual brightness, contrast, saturation, and "
        "gamma adjustment via the Shade Boost shader."));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_brightness", "Shade Boost — Brightness", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Brightness multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% blacks out the image; 100% is double "
        "brightness. Standalone PCSX2 exposes a 1-100% slider; libretro "
        "offers enumerated stops.",
        "pcsx2_shade_boost"));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_contrast", "Shade Boost — Contrast", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Contrast multiplier when Shade Boost is enabled. 50% is neutral "
        "(no change). Standalone PCSX2 exposes a 1-100% slider; libretro "
        "offers enumerated stops.",
        "pcsx2_shade_boost"));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_saturation", "Shade Boost — Saturation", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Color-saturation multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% produces grayscale. Standalone PCSX2 "
        "exposes a 1-100% slider; libretro offers enumerated stops.",
        "pcsx2_shade_boost"));

    s.append(gopt(
        "Post-Processing", "Filters",
        "pcsx2_shade_boost_gamma", "Shade Boost — Gamma", "50",
        {{"0%", "0"},
         {"25%", "25"},
         {"50% (Default — Neutral)", "50"},
         {"75%", "75"},
         {"100%", "100"}},
        "Gamma-correction multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); lower values darken midtones, higher values "
        "brighten midtones. Standalone PCSX2 exposes a 1-100% slider; "
        "libretro offers enumerated stops.",
        "pcsx2_shade_boost"));

    // ── Graphics > On-Screen Display (Phase 4 Task 6) ───────────────────
    //
    // 23 knobs mirroring standalone PCSX2's Graphics/On-Screen Display
    // sub-tab. 22 stored under [EmuCore/GS]; WarnAboutUnsafeSettings
    // under [EmuCore] (split-section, handled core-side in
    // CoreOptionsGraphics::ApplyDefaults).
    //
    // Five groups: "On-Screen Display" (5 rows, no dependsOn);
    // "Performance Stats" (9 rows, all dependsOn pcsx2_osd_performance_pos!=0);
    // "System Information" (2 rows, both dependsOn pcsx2_osd_performance_pos!=0);
    // "Settings & Inputs" (6 rows, mixed dependsOn — see per-row notes);
    // "Messages" (1 row, dependsOn pcsx2_osd_messages_pos!=0).
    //
    // OsdScale + OsdMargin are int sliders standalone-side; libretro v2
    // is Combo-only, so they use enumerated stops (Task 2 Crop + Task 5
    // CAS Sharpness precedent).
    //
    // dependsOn audit: every key referenced (pcsx2_osd_performance_pos,
    // pcsx2_osd_messages_pos, pcsx2_osd_show_settings,
    // pcsx2_load_texture_replacements) lives within the Graphics card.
    // Cross-category limitation (refreshDependencies findChildren scope)
    // does NOT apply — see memory cross_category_dependson_limitation.

    // -- "On-Screen Display" group (5 rows, no dependsOn) --

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_scale", "OSD Scale", "100",
        {{"50%", "50"},
         {"75%", "75"},
         {"100% (Default)", "100"},
         {"125%", "125"},
         {"150%", "150"},
         {"200%", "200"},
         {"300%", "300"},
         {"500%", "500"}},
        "Global multiplier applied to every OSD overlay. 100% matches "
        "standalone PCSX2's default size. Standalone exposes a "
        "25-500% slider; libretro offers enumerated stops."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_margin", "OSD Margin", "10",
        {{"0px", "0"},
         {"5px", "5"},
         {"10px (Default)", "10"},
         {"15px", "15"},
         {"20px", "20"},
         {"30px", "30"},
         {"50px", "50"},
         {"100px", "100"}},
        "Pixel offset between the OSD elements and the screen edge. "
        "Standalone exposes a 0-100px slider; libretro offers "
        "enumerated stops."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_messages_pos", "OSD Messages Position", "1",
        {{"None", "0"},
         {"Top Left (Default)", "1"},
         {"Top Center", "2"},
         {"Top Right", "3"},
         {"Center Left", "4"},
         {"Center", "5"},
         {"Center Right", "6"},
         {"Bottom Left", "7"},
         {"Bottom Center", "8"},
         {"Bottom Right", "9"}},
        "Corner where transient messages (save-state loaded, shader "
        "reload, etc.) are drawn. Set to None to hide them entirely."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_performance_pos", "OSD Performance Position", "3",
        {{"None", "0"},
         {"Top Left", "1"},
         {"Top Center", "2"},
         {"Top Right (Default)", "3"},
         {"Center Left", "4"},
         {"Center", "5"},
         {"Center Right", "6"},
         {"Bottom Left", "7"},
         {"Bottom Center", "8"},
         {"Bottom Right", "9"}},
        "Corner where the performance stats column (FPS/Speed/CPU/GPU/"
        "etc.) is drawn. Set to None to hide the column and grey out "
        "every Performance Stats / System Information toggle."));

    s.append(gopt(
        "On-Screen Display", "On-Screen Display",
        "pcsx2_osd_bold_text", "OSD Text Style (Bold)", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Renders OSD text in bold. Easier to read on bright scenes."));

    // -- "Performance Stats" group (9 rows, all dependsOn pcsx2_osd_performance_pos!=0) --

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_speed", "Show Speed Percentages", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the emulation speed as a percentage. Red below 95%, "
        "green above 105%.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_fps", "Show FPS", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the current frame rate reported by the GS. Useful "
        "for spotting performance issues.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_vps", "Show VPS", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays vertical syncs per second — the PS2 display refresh "
        "reported by the GS.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_resolution", "Show Resolution", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the PS2 internal render resolution and interlacing "
        "mode.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_gs_stats", "Show GS Statistics", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays per-frame GS statistics: draw-call count, VRAM use, "
        "and a frame-time summary.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_cpu", "Show CPU Usage", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays per-component CPU usage (EE, GS, VU).",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_gpu", "Show GPU Usage", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays GPU usage percentage and frame time in milliseconds.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_indicators", "Show Status Indicators", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Displays icons for pause, fast-forward, slow-motion, and "
        "turbo modes in the top-right corner.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Performance Stats",
        "pcsx2_osd_show_frame_times", "Show Frame Times", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays a rolling graph of recent frame times to visualise "
        "stutter.",
        "pcsx2_osd_performance_pos!=0"));

    // -- "Settings & Inputs" group (6 rows, mixed dependsOn) --

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_settings", "Show Settings", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays a compact summary of active emulation settings in "
        "the bottom-right corner.",
        "pcsx2_osd_messages_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_patches", "Show Patches", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Appends active patches (widescreen, no-interlacing, etc.) to "
        "the settings line. Requires Show Settings to be enabled.",
        "pcsx2_osd_show_settings"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_inputs", "Show Inputs", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the current controller input state at the "
        "bottom-left corner."));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_video_capture", "Show Video Capture Status", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Displays a recording indicator while video capture is "
        "active. (Inert in the libretro variant — no FFmpeg capture "
        "is driven from this build.)"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_input_rec", "Show Input Recording Status", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Displays an indicator while input recording is active. "
        "(Inert in the libretro variant — input recording UI is not "
        "driven from this build.)"));

    s.append(gopt(
        "On-Screen Display", "Settings & Inputs",
        "pcsx2_osd_show_texture_replacements", "Show Texture Replacement Status", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays an indicator when replacement textures are loaded "
        "for the current game. Requires Texture Replacement → Load "
        "Textures to be enabled.",
        "pcsx2_load_texture_replacements"));

    // -- "System Information" group (2 rows, both dependsOn pcsx2_osd_performance_pos!=0) --

    s.append(gopt(
        "On-Screen Display", "System Information",
        "pcsx2_osd_show_hardware_info", "Show Hardware Info", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the CPU and GPU model names as two lines in the "
        "performance column.",
        "pcsx2_osd_performance_pos!=0"));

    s.append(gopt(
        "On-Screen Display", "System Information",
        "pcsx2_osd_show_version", "Show PCSX2 Version", "disabled",
        {{"Enabled", "enabled"}, {"Disabled (Default)", "disabled"}},
        "Displays the PCSX2 version string in the performance column.",
        "pcsx2_osd_performance_pos!=0"));

    // -- "Messages" group (1 row, dependsOn pcsx2_osd_messages_pos!=0) --

    s.append(gopt(
        "On-Screen Display", "Messages",
        "pcsx2_warn_about_unsafe_settings", "Warn About Unsafe Settings", "enabled",
        {{"Enabled (Default)", "enabled"}, {"Disabled", "disabled"}},
        "Shows a startup warning if any unsafe settings are enabled.",
        "pcsx2_osd_messages_pos!=0"));

    // Media Capture sub-tab is deliberately skipped: RetroNest's host
    // application owns the screenshot + video-recording pipeline (libretro
    // frontends are responsible for capture; PCSX2's own capture code path
    // is inert when running inside a libretro shell). Same architectural
    // reasoning as the silently-omitted Achievements card (RetroNest owns
    // rcheevos host-side).

    return s;
}

// Preview wiring. The preview widgets
// (cpp/src/ui/settings/widgets/preview/{aspect_ratio_preview,osd_preview}.{h,cpp})
// are adapter-agnostic; they expose Qt properties named exactly as the
// values in keyToProperty below, and
// GenericSettingsPage::wirePreviewBinding routes schema-row changes into
// those properties via Qt's meta-object system.
//
// Empty PreviewSpec from every other (category, subcategory) — no preview
// pane rendered. See memory/sp7c_followup_ui_parity.md for the broader
// Phase 5 motivation.
PreviewSpec Pcsx2LibretroAdapter::previewSpec(const QString& category,
                                              const QString& subcategory) const {
    if (category == "Recommended" && subcategory.isEmpty()) {
        return {"aspect", {
            {"pcsx2_aspect_ratio", "aspectMode"},
        }};
    }
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"pcsx2_osd_show_fps",                  "showFps"},
            {"pcsx2_osd_show_speed",                "showSpeed"},
            {"pcsx2_osd_show_vps",                  "showVps"},
            {"pcsx2_osd_show_resolution",           "showResolution"},
            {"pcsx2_osd_show_cpu",                  "showCpu"},
            {"pcsx2_osd_show_gpu",                  "showGpu"},
            {"pcsx2_osd_show_settings",             "showSettings"},
            {"pcsx2_osd_show_patches",              "showPatches"},
            {"pcsx2_osd_show_inputs",               "showInputs"},
            {"pcsx2_osd_show_frame_times",          "showFrameTimes"},
            {"pcsx2_osd_show_indicators",           "showIndicators"},
            {"pcsx2_osd_show_gs_stats",             "showGsStats"},
            {"pcsx2_osd_show_hardware_info",        "showHardwareInfo"},
            {"pcsx2_osd_show_version",              "showVersion"},
            {"pcsx2_osd_show_video_capture",        "showVideoCapture"},
            {"pcsx2_osd_show_input_rec",            "showInputRec"},
            {"pcsx2_osd_show_texture_replacements", "showTextureReplacements"},
            {"pcsx2_osd_messages_pos",              "messagesPos"},
            {"pcsx2_osd_performance_pos",           "performancePos"},
            {"pcsx2_osd_scale",                     "osdScale"},
        }};
    }
    return {};
}

QVector<SettingsHubCard> Pcsx2LibretroAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width across 3 columns.
        {QStringLiteral("\U0001F4A1"), "Recommended",
         "GS renderer, multi-threaded VU1, fast boot",
         "Recommended", 0, 0, 1, 3},
        // Row 1: Emulation · Graphics · Audio
        // 🎨 palette glyph chosen because the Graphics card covers all five
        // sub-tabs (Display / Rendering / Texture Replacement / Post-Processing
        // / On-Screen Display), not just Display — clearer than 🖥️ at card level.
        {QStringLiteral("\U0001F3AE"), "Emulation",
         "Speed control, system, frame pacing",
         "Emulation", 1, 0},
        {QStringLiteral("\U0001F3A8"), "Graphics",
         "Aspect ratio, upscaling, post-FX, OSD, textures",
         "Graphics", 1, 1},
        {QStringLiteral("\U0001F50A"), "Audio",
         "Volume, mute, buffer, sync mode",
         "Audio", 1, 2},
        // Row 2: Memory Cards
        {QStringLiteral("\U0001F4BE"), "Memory Cards",
         "Slot 1/2 enables, Multitap slots",
         "Memory Cards", 2, 0},
    };
}
