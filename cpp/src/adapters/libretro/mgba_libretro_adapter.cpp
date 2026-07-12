#include "mgba_libretro_adapter.h"
#include "core/path_overrides_store.h"
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

void MgbaLibretroAdapter::prepareCoreEnvironment() const {
    // mGBA's stock mCoreConfigDirectory() has no macOS branch, so it falls
    // through to the XDG/Linux path and unconditionally mkdir's ~/.config/mgba
    // on every load — writing nothing into it, but leaving an empty dir outside
    // RetroNest's portable {root}. The fork is a zero-patch upstream mirror, so
    // this must be fixed frontend-side. mCoreConfigDirectory honors an absolute
    // XDG_CONFIG_HOME first and appends "/mgba"; point it at {root}/emulators so
    // the config dir resolves to the already-existing {root}/emulators/mgba
    // (its mkdir becomes a no-op) — nothing lands in ~/.config.
    qputenv("XDG_CONFIG_HOME", Paths::emulatorsDir().toUtf8());
}

// Packet 7 Stage 2: the schema is rendered from the core's declared option
// table (declared_options.json sidecar / CoreProber) merged with this
// curation overlay — keys, value sets, defaults, labels, and tooltips all
// come from the core. The overlay only routes options into UI categories
// (entry order = per-category row order; cross-listing into "Recommended"
// duplicates the row, both persisting to the same OptionsStore key).
QVector<OptionOverlay> MgbaLibretroAdapter::optionOverlays() const {
    auto ov = [](const QString& key, const QStringList& categories) {
        OptionOverlay o;
        o.key = key;
        for (const auto& c : categories)
            o.placements.append({ c, {}, {} });
        return o;
    };
    // NOTE (Packet 7 Stage 2 conversion finding): the hand-written schema
    // also listed mgba_color_correction, mgba_interframe_blending,
    // mgba_frameskip_threshold, and mgba_frameskip_interval — the 0.11-dev
    // core (rebuilt from upstream master, packet 6) no longer declares any
    // of them, so those rows were DEAD UI (values silently dropped at
    // session start). Deliberately not carried over; if upstream re-adds
    // them, they reappear in the declared table and can be re-curated.
    return {
        // System (skip_bios cross-listed into Recommended, position 1 there)
        ov("mgba_gb_model",              { "System" }),
        ov("mgba_use_bios",              { "System" }),
        ov("mgba_skip_bios",             { "Recommended", "System" }),
        // Video
        ov("mgba_gb_colors",             { "Video" }),
        ov("mgba_gb_colors_preset",      { "Video" }),
        ov("mgba_sgb_borders",           { "Video" }),
        // Emulation (idle/frameskip cross-listed into Recommended)
        ov("mgba_idle_optimization",     { "Recommended", "Emulation" }),
        ov("mgba_frameskip",             { "Recommended", "Emulation" }),
        // Audio
        ov("mgba_audio_low_pass_filter", { "Audio" }),
        ov("mgba_audio_low_pass_range",  { "Audio" }),
        // Input
        ov("mgba_allow_opposing_directions", { "Input" }),
        ov("mgba_solar_sensor_level",    { "Input" }),
        ov("mgba_force_gbp",             { "Input" }),
    };
}

// Frontend-managed rows: aspect ratio and integer scale live in
// frontend.json, not the core's options.json — the mGBA core exposes no
// aspect setting; it is a frontend concern. Cross-listed in Recommended +
// Video (same keys, edits persist to the same value). Combo values are our
// internal strings ("native", "4_3") — AspectRatioPreview::fromSchemaValue
// handles both value sets.
QVector<SettingDef> MgbaLibretroAdapter::extraSettings() const {
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

    const QString aspectTip =
        "How the emulated frame is fitted into the window. "
        "'Native' preserves the system's natural aspect ratio. "
        "'Square Pixel' shows pixel-perfect 1:1. "
        "'4:3' / '16:9' force a TV-style aspect. "
        "'Stretch' fills the window ignoring aspect.";
    const QString integerTip =
        "Snap the displayed frame to the largest integer multiple "
        "of native resolution that fits. Eliminates pixel shimmer "
        "at the cost of some unused screen area.";
    const QStringList aspectVals{ "native", "square", "4_3", "16_9", "stretch" };

    return {
        frontend("aspect_mode", "Aspect Ratio", "native", aspectVals, "Recommended", aspectTip),
        frontend("integer_scale", "Integer Scale", "OFF", { "OFF", "ON" }, "Recommended", integerTip),
        frontend("aspect_mode", "Aspect Ratio", "native", aspectVals, "Video", aspectTip),
        frontend("integer_scale", "Integer Scale", "OFF", { "OFF", "ON" }, "Video", integerTip),
    };
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


QString MgbaLibretroAdapter::findResumeFile(const QString& serial) const {
    if (serial.isEmpty())
        return {};
    // Override applies to all mGBA system variants (gba/gb/gbc) — search
    // the override first if set; else fall back to per-system defaults.
    // Mirror of the write side in GameSession::terminate / libretroSlotPath.
    const QString override = PathOverridesStore::instance().read("mgba", "SaveStates");
    if (!override.isEmpty()) {
        QDir d(override);
        const auto entries = d.entryList({ serial + ".resume" }, QDir::Files);
        if (!entries.isEmpty())
            return d.absoluteFilePath(entries.first());
        return {};   // Override set but no resume file found — don't fall back.
    }
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

QVector<SettingsHubCard> MgbaLibretroAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width curated card, mirrors the
        // Dolphin / PCSX2 / DuckStation / PPSSPP layout convention.
        {QStringLiteral("\U0001F4A1"), "Recommended",
         "Most-tweaked settings — performance, visuals, BIOS",
         "Recommended", 0, 0, 1, 3},
        // Row 1: System · Video · Audio
        {QStringLiteral("\U0001F4BE"), "System",
         "BIOS, Game Boy model",
         "System", 1, 0},
        {QStringLiteral("\U0001F5BC"), "Video",
         "Palettes, SGB borders",
         "Video", 1, 1},
        {QStringLiteral("\U0001F50A"), "Audio",
         "Low-pass filter",
         "Audio", 1, 2},
        // Row 2: Input · Emulation
        // Controller mapping lives on the dedicated "Controller Mapping" entry
        // surfaced from EmulatorDetailPage — not duplicated here.
        {QStringLiteral("\U0001F3AE"), "Input",
         "Solar sensor, GBP rumble, opposing input",
         "Input", 2, 0},
        {QStringLiteral("\U000026A1"), "Emulation",
         "Idle-loop removal, frameskip",
         "Emulation", 2, 1},
    };
}
