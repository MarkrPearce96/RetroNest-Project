#include "dolphin_libretro_adapter.h"

#include "core/ini_file.h"
#include "core/path_overrides_store.h"
#include "core/paths.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

int DolphinLibretroAdapter::raConsoleId(const QString& systemId) const {
    if (systemId == "gc")
        return 16;
    if (systemId == "wii")
        return 19;
    return 0;
}

QString DolphinLibretroAdapter::findResumeFile(const QString& key) const {
    if (key.isEmpty())
        return {};
    // GameSession::terminate writes "<serial-or-basename>.resume" under the
    // SaveStates path override, else emulators/dolphin/<gc|wii>/savestates.
    // Dolphin spans two systems, so search both (mirrors
    // MgbaLibretroAdapter::findResumeFile, which searches gba/gb/gbc). Without
    // this override the base returns {} and Save & Quit -> Resume silently
    // no-ops — the .resume file is written but never read back.
    const QString override = PathOverridesStore::instance().read("dolphin", "SaveStates");
    if (!override.isEmpty()) {
        QDir d(override);
        const auto entries = d.entryList({ key + ".resume" }, QDir::Files);
        if (!entries.isEmpty())
            return d.absoluteFilePath(entries.first());
        return {};  // Override set but no resume file found — don't fall back.
    }
    for (const QString& sys : { "gc", "wii" }) {
        const QString dir = Paths::emulatorDataDir("dolphin", sys) + "/savestates";
        QDir d(dir);
        const auto entries = d.entryList({ key + ".resume" }, QDir::Files);
        if (!entries.isEmpty())
            return d.absoluteFilePath(entries.first());
    }
    return {};
}

QString DolphinLibretroAdapter::extractSerial(const QString& romPath) const {
    // GameCube/Wii discs carry a 6-char game ID at disc offset 0 (e.g. "GZ2P01").
    // For raw .iso/.gcm that's the start of the file; for Dolphin's compressed
    // .rvz/.wia the first 0x80 disc bytes are stored verbatim in the WIA/RVZ
    // header (WIAFileHead is 0x48 bytes; WIADisc.disc_header begins 0x10 into the
    // following WIADisc -> file offset 0x58), while the rest of the disc is
    // compressed. The base ISO reader looks for PlayStation's SYSTEM.CNF, so it
    // can read neither layout. Validate the read against the GameCube/Wii magic
    // word so a wrong offset or non-disc file yields no serial instead of garbage.
    QFile f(romPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    qint64 base = 0;
    const QByteArray magic = f.read(4);
    if (magic == QByteArray("RVZ\x01", 4) || magic == QByteArray("WIA\x01", 4))
        base = 0x58;  // the verbatim disc header lives inside the WIA/RVZ header

    if (!f.seek(base))
        return {};
    const QByteArray hdr = f.read(0x20);  // game id [0x00,0x06) + magic words
    if (hdr.size() < 0x20)
        return {};

    const auto be32 = [&hdr](int off) -> quint32 {
        return (quint32(quint8(hdr[off]))     << 24) | (quint32(quint8(hdr[off + 1])) << 16) |
               (quint32(quint8(hdr[off + 2])) <<  8) |  quint32(quint8(hdr[off + 3]));
    };
    constexpr quint32 GC_MAGIC  = 0xC2339F3D;  // GameCube, disc offset 0x1C
    constexpr quint32 WII_MAGIC = 0x5D1C9EA3;  // Wii, disc offset 0x18
    if (be32(0x1C) != GC_MAGIC && be32(0x18) != WII_MAGIC)
        return {};  // not a recognizable GameCube/Wii disc header

    const QByteArray id = hdr.left(6);
    for (const char c : id) {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!ok)
            return {};  // game id must be printable alphanumeric
    }
    return QString::fromLatin1(id);
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

// Packet 7 Stage 2: the schema is rendered from the core's declared option
// table (declared_options.json sidecar / CoreProber) merged with this
// curation overlay — keys, value sets, value labels, defaults, and wording
// all come from the core (which this fork authors via CoreOptions*.cpp, so
// the two sides can no longer drift; the pre-conversion parity diff was
// 90/90 keys with zero default or value-set differences). The overlay
// carries only UI routing: Graphics sub-tab + group box, the flat
// category/group pages, the 16 Recommended cross-listings, and the two
// overclock dependsOn gates. Entry order = per-category row order (natural
// pages preserved exactly; the Recommended card's group boxes follow
// first-encounter order of these entries). Generated mechanically from the
// retired hand-written rows (parity-checked by test_schema_parity).
QVector<OptionOverlay> DolphinLibretroAdapter::optionOverlays() const {
    QVector<OptionOverlay> list;
    auto add = [&list](const QString& key, QVector<OverlayPlacement> places,
                       const QString& dependsOn = QString()) {
        OptionOverlay o;
        o.key = key;
        o.placements = std::move(places);
        o.dependsOn = dependsOn;
        list.append(o);
    };

    add("dolphin_aspect_ratio",
        {{ "Graphics", "General", "General" },
         { "Recommended", "", "Visual Quality" }});
    add("dolphin_vsync",
        {{ "Graphics", "General", "General" }});
    add("dolphin_precision_frame_timing",
        {{ "Graphics", "General", "General" }});
    add("dolphin_shader_compilation",
        {{ "Graphics", "General", "General" },
         { "Recommended", "", "Performance" }});
    add("dolphin_wait_for_shaders",
        {{ "Graphics", "General", "General" },
         { "Recommended", "", "Performance" }});
    add("dolphin_internal_resolution",
        {{ "Graphics", "Enhancements", "Enhancements" },
         { "Recommended", "", "Visual Quality" }});
    add("dolphin_antialiasing",
        {{ "Graphics", "Enhancements", "Enhancements" },
         { "Recommended", "", "Visual Quality" }});
    add("dolphin_texture_filtering",
        {{ "Graphics", "Enhancements", "Enhancements" },
         { "Recommended", "", "Visual Quality" }});
    add("dolphin_output_resampling",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_scaled_efb_copy",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_per_pixel_lighting",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_widescreen_hack",
        {{ "Graphics", "Enhancements", "Enhancements" },
         { "Recommended", "", "Visual Quality" }});
    add("dolphin_force_true_color",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_disable_fog",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_arbitrary_mipmap_detection",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_disable_copy_filter",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_hdr_output",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_stereo_mode",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_stereo_swap_eyes",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_stereo_per_eye_full",
        {{ "Graphics", "Enhancements", "Enhancements" }});
    add("dolphin_texcache_accuracy",
        {{ "Graphics", "Hacks", "Hacks" },
         { "Recommended", "", "Performance Hacks" }});
    add("dolphin_skip_efb_access",
        {{ "Graphics", "Hacks", "Hacks" },
         { "Recommended", "", "Performance Hacks" }});
    add("dolphin_ignore_format_changes",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_store_efb_to_texture",
        {{ "Graphics", "Hacks", "Hacks" },
         { "Recommended", "", "Performance Hacks" }});
    add("dolphin_defer_efb_copies",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_gpu_texture_decoding",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_store_xfb_to_texture",
        {{ "Graphics", "Hacks", "Hacks" },
         { "Recommended", "", "Performance Hacks" }});
    add("dolphin_immediate_xfb",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_skip_duplicate_xfbs",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_fast_depth_calc",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_disable_bounding_box",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_vertex_rounding",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_save_texcache_to_state",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_vbi_skip",
        {{ "Graphics", "Hacks", "Hacks" }});
    add("dolphin_load_custom_textures",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_prefetch_custom_textures",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_enable_graphics_mods",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_crop",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_backend_multithreading",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_prefer_vs_expansion",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_cpu_cull",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_defer_efb_invalidation",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_manual_texture_sampling",
        {{ "Graphics", "Advanced", "Advanced" }});
    add("dolphin_osd_font_size",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_perf_samp_window",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_osd_messages",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_show_fps",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_show_ftimes",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_show_vps",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_show_vtimes",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_show_speed",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_show_graphs",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_show_speed_colors",
        {{ "Graphics", "On-Screen Display", "On-Screen Display" }});
    add("dolphin_dsp_engine",
        {{ "Audio", "", "DSP" },
         { "Recommended", "", "Audio" }});
    add("dolphin_audio_latency",
        {{ "Audio", "", "Backend" }});
    add("dolphin_dpl2_decoder",
        {{ "Audio", "", "Backend" }});
    add("dolphin_dpl2_quality",
        {{ "Audio", "", "Backend" }});
    add("dolphin_audio_buffer_size",
        {{ "Audio", "", "Playback" }});
    add("dolphin_audio_fill_gaps",
        {{ "Audio", "", "Playback" }});
    add("dolphin_audio_preserve_pitch",
        {{ "Audio", "", "Playback" }});
    add("dolphin_audio_mute_on_unthrottle",
        {{ "Audio", "", "Playback" }});
    add("dolphin_volume",
        {{ "Audio", "", "Volume" },
         { "Recommended", "", "Audio" }});
    add("dolphin_cpu_thread",
        {{ "General", "", "Basic" },
         { "Recommended", "", "Performance" }});
    add("dolphin_enable_cheats",
        {{ "General", "", "Basic" },
         { "Recommended", "", "Convenience" }});
    add("dolphin_load_game_into_memory",
        {{ "General", "", "Basic" }});
    add("dolphin_override_region_settings",
        {{ "General", "", "Basic" }});
    add("dolphin_emulation_speed",
        {{ "General", "", "Basic" }});
    add("dolphin_fallback_region",
        {{ "General", "", "Region" }});
    add("dolphin_cpu_core",
        {{ "Advanced", "", "CPU" }});
    add("dolphin_mmu",
        {{ "Advanced", "", "CPU" }});
    add("dolphin_pause_on_panic",
        {{ "Advanced", "", "CPU" }});
    add("dolphin_accurate_cpu_cache",
        {{ "Advanced", "", "CPU" }});
    add("dolphin_correct_time_drift",
        {{ "Advanced", "", "Timing" }});
    add("dolphin_rush_frame_presentation",
        {{ "Advanced", "", "Timing" }});
    add("dolphin_smooth_early_presentation",
        {{ "Advanced", "", "Timing" }});
    add("dolphin_overclock_enable",
        {{ "Advanced", "", "Clock Override" }});
    add("dolphin_overclock",
        {{ "Advanced", "", "Clock Override" }},
        "dolphin_overclock_enable");
    add("dolphin_vi_overclock_enable",
        {{ "Advanced", "", "VBI Override" }});
    add("dolphin_vi_overclock",
        {{ "Advanced", "", "VBI Override" }},
        "dolphin_vi_overclock_enable");
    add("dolphin_skip_ipl",
        {{ "GameCube", "", "IPL" },
         { "Recommended", "", "Convenience" }});
    add("dolphin_gc_language",
        {{ "GameCube", "", "IPL" }});
    add("dolphin_slot_a",
        {{ "GameCube", "", "Devices" }});
    add("dolphin_slot_b",
        {{ "GameCube", "", "Devices" }});
    add("dolphin_serial_port_1",
        {{ "GameCube", "", "Devices" }});
    add("dolphin_wii_keyboard",
        {{ "Wii", "", "Misc" }});
    add("dolphin_enable_wiilink",
        {{ "Wii", "", "Misc" }});
    add("dolphin_wii_sd_card",
        {{ "Wii", "", "SD Card" }});
    add("dolphin_wii_sd_card_writes",
        {{ "Wii", "", "SD Card" }});
    add("dolphin_wii_sd_card_folder_sync",
        {{ "Wii", "", "SD Card" }});
    add("dolphin_wii_sd_card_size",
        {{ "Wii", "", "SD Card" }});

    return list;
}

QVector<SettingsHubCard> DolphinLibretroAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width across all 3 columns (mirrors the
        // PCSX2 hub layout).
        {QStringLiteral("\U00002B50"), "Recommended",
         "The dozen settings that matter most",
         "Recommended", 0, 0, 1, 3},
        // Row 1: Graphics · Audio · General
        {QStringLiteral("\U0001F3A8"), "Graphics",
         "Resolution, AA, enhancements, hacks, OSD",
         "Graphics", 1, 0},
        {QStringLiteral("\U0001F50A"), "Audio",
         "DSP engine, latency, volume, surround",
         "Audio", 1, 1},
        {QStringLiteral("\U00002699\U0000FE0F"), "General",
         "Dual core, cheats, speed limit, region",
         "General", 1, 2},
        // Row 2: Advanced · GameCube · Wii
        {QStringLiteral("\U0001F6E0\U0000FE0F"), "Advanced",
         "CPU core, MMU, overclock, timing",
         "Advanced", 2, 0},
        {QStringLiteral("\U0001F4BE"), "GameCube",
         "IPL, language, memory-card slots, SP1",
         "GameCube", 2, 1},
        {QStringLiteral("\U0001F3AE"), "Wii",
         "USB keyboard, WiiLink, SD card",
         "Wii", 2, 2},
    };
}

PreviewSpec DolphinLibretroAdapter::previewSpec(const QString& category,
                                                const QString& subcategory) const {
    if (category == "Recommended" && subcategory.isEmpty()) {
        // Aspect-ratio preview on the Recommended page (mirrors PCSX2/PPSSPP).
        // Bound to the same dolphin_aspect_ratio key the Recommended card lists.
        return {"aspect", {
            {"dolphin_aspect_ratio", "aspectMode"},
        }};
    }
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
