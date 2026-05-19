#include "mgba_libretro_adapter.h"
#include "core/paths.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

QVector<BiosDef> MgbaLibretroAdapter::biosFiles() const {
    return {
        { "gba_bios.bin", "GBA BIOS (optional, recommended)", false, "" },
    };
}

QVector<PathDef> MgbaLibretroAdapter::pathsDefs() const {
    // Screenshots row dropped — RetroNest has no gameplay-screenshot
    // capture, so an override would be UI for a feature that doesn't
    // exist. Re-add when/if screenshot capture lands.
    return {
        { "Saves",       "libretro", "Saves",      "saves",      PathBase::EmulatorData },
        { "Save States", "libretro", "SaveStates", "savestates", PathBase::EmulatorData },
    };
}

ResolutionOptions MgbaLibretroAdapter::resolutionOptions() const {
    return {};
}

AspectRatioOptions MgbaLibretroAdapter::aspectRatioOptions() const {
    return {};
}

QVector<ControllerTypeDef> MgbaLibretroAdapter::controllerTypes() const {
    return {
        { "GBA", "GBA Controller",
          ":/AppUI/qml/AppUI/images/controllers/Gameboy.svg" },
    };
}

QVector<BindingDef> MgbaLibretroAdapter::controllerBindingDefsForType(const QString&) const {
    // Spotlight coordinates target the Gameboy.svg viewBox (822 × 1354).
    // L/R don't exist on the DMG-01 silhouette so they get no spotlight
    // (zero coords suppress the overlay).
    return {
        // D-Pad — cross at lower-left
        { BindingDef::Button, "D-Pad Up",    "D-Pad",     "Pad1", "Up",     "",
          "DPad",        180,  825, 65 },
        { BindingDef::Button, "D-Pad Down",  "D-Pad",     "Pad1", "Down",   "",
          "DPad",        180,  975, 65 },
        { BindingDef::Button, "D-Pad Left",  "D-Pad",     "Pad1", "Left",   "",
          "DPad",        105,  900, 65 },
        { BindingDef::Button, "D-Pad Right", "D-Pad",     "Pad1", "Right",  "",
          "DPad",        255,  900, 65 },
        // Action buttons — A right, B diagonal-down-left of A
        { BindingDef::Button, "A",           "Buttons",   "Pad1", "A",      "",
          "FaceButtons", 720,  875, 45 },
        { BindingDef::Button, "B",           "Buttons",   "Pad1", "B",      "",
          "FaceButtons", 590,  925, 45 },
        // Shoulders — not present on DMG silhouette; no spotlight.
        { BindingDef::Button, "L",           "Shoulders", "Pad1", "L",      "",
          "Shoulders",     0,    0,  0 },
        { BindingDef::Button, "R",           "Shoulders", "Pad1", "R",      "",
          "Shoulders",     0,    0,  0 },
        // Select / Start — small angled buttons in lower-center
        { BindingDef::Button, "Start",       "System",    "Pad1", "Start",  "",
          "System",      460, 1140, 50 },
        { BindingDef::Button, "Select",      "System",    "Pad1", "Select", "",
          "System",      325, 1140, 50 },
    };
}

QVector<HotkeyDef> MgbaLibretroAdapter::hotkeyBindingDefs() const {
    return {};
}

QVector<QPair<QString, QString>> MgbaLibretroAdapter::frontendSettingDefaults() const {
    return {
        { "aspect_mode",    "native" },
        { "integer_scale",  "OFF"    },
    };
}

QVector<SettingDef> MgbaLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    auto opt = [](const QString& key, const QString& label,
                  const QString& def, const QStringList& vals,
                  const QString& category, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.type = SettingDef::Combo;
        for (const auto& v : vals)
            d.options.append({ v, v });
        d.category = category;
        d.tooltip = tooltip;
        return d;
    };

    // Helper for FrontendSetting entries: same Combo shape but backed by
    // frontend.json rather than options.json. Combo values are our internal
    // strings (e.g. "native", "4_3") — NOT the AspectRatioPreview strings.
    // AspectRatioPreview::fromSchemaValue is extended to handle both sets.
    auto frontend = [](const QString& key, const QString& label,
                       const QString& def, const QStringList& vals,
                       const QString& category, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::FrontendSetting;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.type = SettingDef::Combo;
        for (const auto& v : vals)
            d.options.append({ v, v });
        d.category = category;
        d.tooltip = tooltip;
        return d;
    };

    // Recommended — curated short-list, same keys as their native categories
    // below. Edits in either place persist to the same OptionsStore key.

    // Frontend-managed: aspect ratio and integer scale (Recommended + Video).
    // These live in frontend.json, not the libretro core's options.json, because
    // the mGBA core exposes no aspect-ratio setting — it is a frontend concern.
    s << frontend("aspect_mode", "Aspect Ratio",
                  "native",
                  { "native", "square", "4_3", "16_9", "stretch" },
                  "Recommended",
                  "How the emulated frame is fitted into the window. "
                  "'Native' preserves the system's natural aspect ratio. "
                  "'Square Pixel' shows pixel-perfect 1:1. "
                  "'4:3' / '16:9' force a TV-style aspect. "
                  "'Stretch' fills the window ignoring aspect.");

    s << frontend("integer_scale", "Integer Scale",
                  "OFF",
                  { "OFF", "ON" },
                  "Recommended",
                  "Snap the displayed frame to the largest integer multiple "
                  "of native resolution that fits. Eliminates pixel shimmer "
                  "at the cost of some unused screen area.");

    s << opt("mgba_skip_bios",
             "Skip BIOS Intro (Restart)",
             "OFF",
             { "OFF", "ON" },
             "Recommended",
             "When using an official BIOS/bootloader, skip the start-up logo animation. This setting is ignored when 'Use BIOS File if Found' is disabled.");
    s << opt("mgba_color_correction",
             "Color Correction",
             "OFF",
             { "OFF", "GBA", "GBC", "Auto" },
             "Recommended",
             "Adjusts output colors to match the display of real GBA/GBC hardware.");
    s << opt("mgba_interframe_blending",
             "Interframe Blending",
             "OFF",
             { "OFF", "mix", "mix_smart", "lcd_ghosting", "lcd_ghosting_fast" },
             "Recommended",
             "Simulates LCD ghosting effects. 'mix' performs a 50:50 mix of the current and previous frames. 'mix_smart' detects screen flickering and only blends affected pixels. 'lcd_ghosting' mimics natural LCD response times.");
    s << opt("mgba_idle_optimization",
             "Idle Loop Removal",
             "Remove Known",
             { "Remove Known", "Detect and Remove", "Don't Remove" },
             "Recommended",
             "Reduce system load by optimizing 'idle-loops' — sections of code where nothing happens but the CPU runs at full speed. Improves performance, especially on low-end hardware.");
    s << opt("mgba_frameskip",
             "Frameskip",
             "disabled",
             { "disabled", "auto", "auto_threshold", "fixed_interval" },
             "Recommended",
             "Skip frames to avoid audio buffer under-run (crackling). Improves performance at the expense of visual smoothness.");

    // System
    s << opt("mgba_gb_model",
             "Game Boy Model (Restart)",
             "Autodetect",
             { "Autodetect", "Game Boy", "Super Game Boy", "Game Boy Color",
               "Super Game Boy Color", "Game Boy Advance" },
             "System",
             "Runs loaded content with a specific Game Boy model. 'Autodetect' will select the most appropriate model for the current game.");
    s << opt("mgba_use_bios",
             "Use BIOS File if Found (Restart)",
             "ON",
             { "ON", "OFF" },
             "System",
             "Use official BIOS/bootloader for emulated hardware, if present in RetroArch's system directory.");
    s << opt("mgba_skip_bios",
             "Skip BIOS Intro (Restart)",
             "OFF",
             { "OFF", "ON" },
             "System",
             "When using an official BIOS/bootloader, skip the start-up logo animation. This setting is ignored when 'Use BIOS File if Found' is disabled.");

    // Video
    // Frontend-managed aspect controls duplicated here so they appear under
    // the Video category too. Both entries share the same frontend.json keys,
    // so editing from either category persists to the same value.
    s << frontend("aspect_mode", "Aspect Ratio",
                  "native",
                  { "native", "square", "4_3", "16_9", "stretch" },
                  "Video",
                  "How the emulated frame is fitted into the window. "
                  "'Native' preserves the system's natural aspect ratio. "
                  "'Square Pixel' shows pixel-perfect 1:1. "
                  "'4:3' / '16:9' force a TV-style aspect. "
                  "'Stretch' fills the window ignoring aspect.");

    s << frontend("integer_scale", "Integer Scale",
                  "OFF",
                  { "OFF", "ON" },
                  "Video",
                  "Snap the displayed frame to the largest integer multiple "
                  "of native resolution that fits. Eliminates pixel shimmer "
                  "at the cost of some unused screen area.");

    s << opt("mgba_gb_colors",
             "Default Game Boy Palette",
             "Grayscale",
             { "Grayscale" },
             "Video",
             "Selects which palette is used for Game Boy games that are not Game Boy Color or Super Game Boy compatible, or if the model is forced to Game Boy.");
    s << opt("mgba_gb_colors_preset",
             "Hardware Preset Game Boy Palettes (Restart)",
             "0",
             { "0", "1", "2", "3" },
             "Video",
             "Use the palettes for Game Boy games that have presets on the Game Boy Color or Super Game Boy.");
    s << opt("mgba_sgb_borders",
             "Use Super Game Boy Borders (Restart)",
             "ON",
             { "ON", "OFF" },
             "Video",
             "Display Super Game Boy borders when running Super Game Boy enhanced games.");
    s << opt("mgba_color_correction",
             "Color Correction",
             "OFF",
             { "OFF", "GBA", "GBC", "Auto" },
             "Video",
             "Adjusts output colors to match the display of real GBA/GBC hardware.");
    s << opt("mgba_interframe_blending",
             "Interframe Blending",
             "OFF",
             { "OFF", "mix", "mix_smart", "lcd_ghosting", "lcd_ghosting_fast" },
             "Video",
             "Simulates LCD ghosting effects. 'Simple' performs a 50:50 mix of the current and previous frames. 'Smart' attempts to detect screen flickering, and only performs a 50:50 mix on affected pixels. 'LCD Ghosting' mimics natural LCD response times by combining multiple buffered frames. 'Simple' or 'Smart' blending is required when playing games that aggressively exploit LCD ghosting for transparency effects (Wave Race, Chikyuu Kaihou Gun ZAS, F-Zero, the Boktai series...).");

    // Audio
    s << opt("mgba_audio_low_pass_filter",
             "Audio Filter",
             "disabled",
             { "disabled", "enabled" },
             "Audio",
             "Enables a low pass audio filter to reduce the 'harshness' of generated audio.");
    s << opt("mgba_audio_low_pass_range",
             "Audio Filter Level",
             "60",
             { "5", "10", "15", "20", "25", "30", "35", "40", "45", "50",
               "55", "60", "65", "70", "75", "80", "85", "90", "95" },
             "Audio",
             "Specifies the cut-off frequency of the low pass audio filter. A higher value increases the perceived 'strength' of the filter, since a wider range of the high frequency spectrum is attenuated.");

    // Input
    s << opt("mgba_allow_opposing_directions",
             "Allow Opposing Directional Input",
             "no",
             { "no", "yes" },
             "Input",
             "Enabling this will allow pressing / quickly alternating / holding both left and right (or up and down) directions at the same time. This may cause movement-based glitches.");
    s << opt("mgba_solar_sensor_level",
             "Solar Sensor Level",
             "0",
             { "sensor", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" },
             "Input",
             "Sets ambient sunlight intensity. Can be used by games that included a solar sensor in their cartridges, e.g: the Boktai series.");
    s << opt("mgba_force_gbp",
             "Game Boy Player Rumble (Restart)",
             "OFF",
             { "OFF", "ON" },
             "Input",
             "Enabling this will allow compatible games with the Game Boy Player boot logo to make the controller rumble. Due to how Nintendo decided this feature should work, it may cause glitches such as flickering or lag in some of these games.");

    // Emulation (upstream category key: "performance")
    s << opt("mgba_idle_optimization",
             "Idle Loop Removal",
             "Remove Known",
             { "Remove Known", "Detect and Remove", "Don't Remove" },
             "Emulation",
             "Reduce system load by optimizing so-called 'idle-loops' - sections in the code where nothing happens, but the CPU runs at full speed (like a car revving in neutral). Improves performance, and should be enabled on low-end hardware.");
    s << opt("mgba_frameskip",
             "Frameskip",
             "disabled",
             { "disabled", "auto", "auto_threshold", "fixed_interval" },
             "Emulation",
             "Skip frames to avoid audio buffer under-run (crackling). Improves performance at the expense of visual smoothness. 'Auto' skips frames when advised by the frontend. 'Auto (Threshold)' utilises the 'Frameskip Threshold (%)' setting. 'Fixed Interval' utilises the 'Frameskip Interval' setting.");
    s << opt("mgba_frameskip_threshold",
             "Frameskip Threshold (%)",
             "33",
             { "15", "18", "21", "24", "27", "30", "33", "36", "39", "42",
               "45", "48", "51", "54", "57", "60" },
             "Emulation",
             "When 'Frameskip' is set to 'Auto (Threshold)', specifies the audio buffer occupancy threshold (percentage) below which frames will be skipped. Higher values reduce the risk of crackling by causing frames to be dropped more frequently.");
    s << opt("mgba_frameskip_interval",
             "Frameskip Interval",
             "0",
             { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" },
             "Emulation",
             "When 'Frameskip' is set to 'Fixed Interval', the value set here is the number of frames omitted after a frame is rendered - i.e. '0' = 60fps, '1' = 30fps, '2' = 15fps, etc.");

    return s;
}

PreviewSpec MgbaLibretroAdapter::previewSpec(const QString& category,
                                              const QString& subcategory) const {
    // Recommended and Video pages both host a live AspectRatioPreview bound
    // to aspect_mode. The combo values ("native", "square", "4_3", "16_9",
    // "stretch") are translated by AspectRatioPreview::fromSchemaValue to the
    // widget's AspectRatio enum. integerScaling is also wired so the preview
    // reflects integer-scale snapping.
    if (category == "Recommended" && subcategory.isEmpty())
        return { "aspect", { { "aspect_mode", "aspectMode" } } };
    return {};
}

QString MgbaLibretroAdapter::configFilePath() const {
    // Libretro adapters have no INI; settings UI dispatches via SettingDef::Storage.
    return {};
}

QString MgbaLibretroAdapter::extractSerial(const QString& romPath) const {
    QFile f(romPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QString ext = QFileInfo(romPath).suffix().toLower();
    if (ext == "gba") {
        // GBA cartridge header: game code at 0xAC, 4 bytes (e.g. "AAME")
        // 0xA0 is the Game Title (12 bytes) — NOT the serial.
        f.seek(0xAC);
        QByteArray code = f.read(4);
        return QString::fromLatin1(code).trimmed();
    }
    if (ext == "gb" || ext == "gbc") {
        // GB/GBC cartridge header: title at 0x0134, up to 16 bytes
        f.seek(0x0134);
        QByteArray title = f.read(16);
        return QString::fromLatin1(title.split('\0').first()).trimmed();
    }
    return {};
}

int MgbaLibretroAdapter::raConsoleId(const QString& systemId) const {
    if (systemId == "gba") return 5;
    if (systemId == "gb")  return 4;
    if (systemId == "gbc") return 6;
    return 0;
}

QString MgbaLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    for (const QString& sys : { "gba", "gb", "gbc" }) {
        const QString dir = Paths::emulatorDataDir("mgba", sys) + "/savestates";
        QDir d(dir);
        // Filter directly by serial: GameSession::terminate writes "{serial}.resume"
        const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
        if (!entries.isEmpty())
            return d.absoluteFilePath(entries.first());
    }
    return {};
}
