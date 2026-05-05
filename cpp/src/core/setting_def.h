#pragma once

#include <functional>
#include <QString>
#include <QVector>
#include <QPair>

/**
 * SettingDef — describes a single emulator setting for UI rendering.
 */
struct SettingDef {
    enum Type { Bool, Int, Float, String, Combo };

    QString category;    // UI tab/group (e.g. "Graphics", "Audio")
    QString subcategory; // Sub-tab within category (e.g. "Display", "Rendering")
    QString group;       // Group box title (empty = no group box)
    QString section;     // INI section (e.g. "EmuCore/GS")
    QString key;         // INI key (e.g. "upscale_multiplier")
    QString label;       // Display label
    QString tooltip;     // Optional tooltip
    Type type = String;
    QString defaultValue;

    // For Combo: list of (display label, INI value) pairs
    QVector<QPair<QString, QString>> options;

    // For Int/Float: range
    double minVal = 0;
    double maxVal = 100;
    double step = 1;

    // Layout hint: "paired" = render in 2-column grid, "slider" = horizontal slider
    QString layout;

    // Unit suffix displayed next to value (e.g. "ms", "%")
    QString suffix;

    // If non-empty, names another setting on the same page that gates this one.
    // The master may be a Bool (active = true) or a Combo (active when its
    // value is not in {"", "0", "false", "Disabled", "None"}, case-insensitive).
    // When the master transitions to inactive, dependent rows dim, their inner
    // controls are disabled, and their values reset to schema defaults.
    //
    // Also accepts a small boolean expression for multi-master gates:
    //   - "Foo"             — Foo is truthy (existing behavior)
    //   - "!Foo"            — Foo is falsy
    //   - "Foo=Bar"         — Foo's current value equals "Bar"
    //   - "Foo!=Bar"        — Foo's current value does not equal "Bar"
    //   - "A && B"          — all terms true
    //   - "A || B"          — any term true
    // No parentheses; '&&' and '||' may not mix in one expression. See
    // setting_dependency.h::evaluateDependencyExpression.
    QString dependsOn;

    // If non-zero, this Bool setting reads/writes a single bit of an
    // int-valued INI key. The widget displays as a checkbox; on save the
    // bit is set/cleared in the existing int and the full int is written
    // back. Used by PPSSPP for iShowStatusFlags. Default 0 = normal Bool.
    int bitmask = 0;

    // If true, the displayed checkbox state is the inverse of the stored
    // INI value. The label phrases the setting in the negative — e.g.
    // upstream Dolphin's "Disable Bounding Box" is `inverted=true` over
    // `BBoxEnable`: checked = stored False = bounding-box disabled.
    // Mirrors Dolphin's `ConfigBool(label, key, layer, /*inverted=*/true)`
    // four-arg constructor (Source/Core/DolphinQt/Config/ConfigControls/
    // ConfigBool.cpp). The underlying INI value is unchanged — only load
    // (`setChecked`) and save (`save("true"/"false")`) flip. Dependency
    // expressions still reason about the stored INI value, not the
    // displayed inverse.
    bool inverted = false;

    // Optional recommended value shown in the new PCSX2 dialog description bar.
    // When empty, UI falls back to displaying defaultValue.
    QString recommendedValue;

    // Optional. If set, GenericSettingsPage invokes this instead of the
    // default AppController::saveSettings() call when the widget value
    // changes. Used for settings whose stored format diverges from the
    // widget value (e.g. percent slider → numerator/denominator pair).
    // Default unset → standard save path.
    //
    // Signature avoids depending on AppController in this header by
    // taking a generic save callback the page passes through.
    using SaveCallback = std::function<void(const QString& section,
                                            const QString& key,
                                            const QString& value)>;
    std::function<void(const QString& widgetValue,
                       const SaveCallback& defaultSave)> saveTransform;

    // Optional inverse of saveTransform — synthesizes the widget value
    // from one or more INI keys when the page first loads. Used when a
    // single combo represents the state of multiple INI keys (e.g.
    // Dolphin's "DSP Emulation Engine" combo binds to both DSPHLE and
    // EnableJIT). Default unset → page reads `section`/`key` directly.
    using LoadCallback = std::function<QString(const QString& section,
                                                const QString& key)>;
    std::function<QString(const LoadCallback& read)> loadTransform;

    // Optional per-key INI file override. When non-empty, ConfigService
    // routes reads/writes for this setting to this absolute file path
    // instead of adapter->configFilePath(). Mirrors IniPatch::iniFilePath.
    // Used by emulators that span multiple config files (e.g. Dolphin's
    // GFX.ini for graphics keys vs Dolphin.ini for everything else).
    QString iniFilePath;
};
