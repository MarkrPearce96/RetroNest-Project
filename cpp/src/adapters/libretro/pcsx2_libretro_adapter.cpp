#include "pcsx2_libretro_adapter.h"

#include "core/binding_def.h"
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
// emulators/pcsx2-libretro/ps2/savestates/. Look there. Base id is
// "pcsx2-libretro" (the manifest id used by Paths::emulatorDataDir on
// the save side at game_session.cpp:392); systemId is "ps2".
QString Pcsx2LibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    const QString dir = Paths::emulatorDataDir("pcsx2-libretro", "ps2") + "/savestates";
    QDir d(dir);
    const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
    if (!entries.isEmpty())
        return d.absoluteFilePath(entries.first());
    return {};
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

    s.append(opt(
        "Recommended", "Emulation",
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
        "Recommended", "Emulation",
        "pcsx2_mtvu", "Multi-Threaded VU1", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Run the VU1 microprogram on its own thread instead of the EE "
        "thread. Compatible with the vast majority of games and "
        "significantly reduces EE-thread saturation on Apple Silicon. "
        "Disable only if a specific game shows MTVU-related glitches. "
        "Takes effect on next launch."));

    s.append(opt(
        "Recommended", "Emulation",
        "pcsx2_fast_boot", "Fast Boot", "enabled",
        {{"Enabled", "enabled"},
         {"Disabled", "disabled"}},
        "Skip the PS2 BIOS Sony intro and region-check screen on launch. "
        "Disable if you want to see the BIOS screen (e.g. to verify your "
        "BIOS region or to use the BIOS browser). Takes effect on next launch."));

    // SP7c Phase 1 — Speed Control (sub-group A of the Emulation card).
    // Three Framerate scalars; values list mirrors the standalone PCSX2
    // dialog exactly (RetroNest's cpp/src/adapters/pcsx2_adapter.cpp:200-218).
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
        "pcsx2_fast_boot_ff", "Fast-Forward Through BIOS", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
        "When Fast Boot is enabled, also fast-forward the brief BIOS boot "
        "animation. No effect when Fast Boot is disabled. Takes effect on "
        "next launch.",
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

    // SP7c Phase 2 — Audio card.
    //
    // 5 rows under category="Audio". Audio routes through
    // LibretroAudioStream (SP4), so Cubeb-only knobs from the standalone
    // dialog (Backend / DriverName / DeviceName / ExpansionMode /
    // OutputLatencyMS / OutputLatencyMinimal) are dropped — they are
    // silently inert in libretro mode. ExpansionMode is additionally
    // forced to Disabled by the core's Settings.cpp because the libretro
    // audio_batch_cb is stereo-only.
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

    // Volume + Fast-Forward Volume share the same 9-stop list. Shared
    // identifier (mirrors the Phase 1 speedOptions precedent at line 133);
    // check_schema_fidelity.py resolves the identifier via HOST_VALUES_REF_RE.
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

    // SP7c Phase 3 — Memory Cards card.
    //
    // 5 rows under category="Memory Cards" mirroring the standalone PCSX2
    // dialog at cpp/src/adapters/pcsx2_adapter.cpp:986-1029. The 2 filename
    // rows (Slot1_Filename / Slot2_Filename) are dropped — libretro core
    // options are Combo-only, not free-form strings, so the filenames stay
    // hardcoded in the core's Settings.cpp.
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

    return s;
}
