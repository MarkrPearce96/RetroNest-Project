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
    return {
        { "Saves",       "", "", "saves",       PathBase::EmulatorData },
        { "Save states", "", "", "savestates",  PathBase::EmulatorData },
        { "Screenshots", "", "", "screenshots", PathBase::EmulatorData },
    };
}

ResolutionOptions MgbaLibretroAdapter::resolutionOptions() const {
    return {};
}

AspectRatioOptions MgbaLibretroAdapter::aspectRatioOptions() const {
    return {};
}

QVector<ControllerTypeDef> MgbaLibretroAdapter::controllerTypes() const {
    return { { "GBA", "GBA Controller", "" } };
}

QVector<BindingDef> MgbaLibretroAdapter::controllerBindingDefsForType(const QString&) const {
    return {
        { BindingDef::Button, "D-Pad Up",    "D-Pad", "", "Up",     "" },
        { BindingDef::Button, "D-Pad Down",  "D-Pad", "", "Down",   "" },
        { BindingDef::Button, "D-Pad Left",  "D-Pad", "", "Left",   "" },
        { BindingDef::Button, "D-Pad Right", "D-Pad", "", "Right",  "" },
        { BindingDef::Button, "A",           "Buttons", "", "A",    "" },
        { BindingDef::Button, "B",           "Buttons", "", "B",    "" },
        { BindingDef::Button, "L",           "Shoulders", "", "L",  "" },
        { BindingDef::Button, "R",           "Shoulders", "", "R",  "" },
        { BindingDef::Button, "Start",       "System", "", "Start", "" },
        { BindingDef::Button, "Select",      "System", "", "Select","" },
    };
}

QVector<HotkeyDef> MgbaLibretroAdapter::hotkeyBindingDefs() const {
    return {};
}

QVector<SettingDef> MgbaLibretroAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    auto opt = [](const QString& key, const QString& label,
                  const QString& def, const QStringList& vals,
                  const QString& group, const QString& tooltip) -> SettingDef {
        SettingDef d;
        d.storage = SettingDef::Storage::LibretroOption;
        d.key = key;
        d.label = label;
        d.defaultValue = def;
        d.type = SettingDef::Combo;
        for (const auto& v : vals)
            d.options.append({ v, v });
        d.category = "Core Options";
        d.group = group;
        d.tooltip = tooltip;
        return d;
    };

    s << opt("mgba_skip_bios", "Skip BIOS intro", "OFF", { "OFF", "ON" },
             "System", "Skip the GBA BIOS intro animation.")
      << opt("mgba_use_bios", "Use BIOS file if available", "ON", { "ON", "OFF" },
             "System", "Load gba_bios.bin from the BIOS folder if present.")
      << opt("mgba_solar_sensor_level", "Solar Sensor level", "0",
             { "0","1","2","3","4","5","6","7","8","9","10" },
             "System", "Solar sensor level for Boktai games.")
      << opt("mgba_color_correction", "Color correction", "OFF",
             { "OFF", "GBA", "GBC", "Auto" },
             "Video", "Apply LCD color correction filters.")
      << opt("mgba_interframe_blending", "Interframe blending", "OFF",
             { "OFF", "Mix", "LCD Ghosting (Accurate)", "LCD Ghosting (Fast)" },
             "Video", "Smooth animation by blending consecutive frames.");

    return s;
}

PreviewSpec MgbaLibretroAdapter::previewSpec(const QString&, const QString&) const {
    return {}; // deferred — wired in a follow-up if we want aspect-ratio preview
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
