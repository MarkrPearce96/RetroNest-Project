#pragma once

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

    // If non-empty, this setting is enabled only when the named key's bool is true.
    // The key is matched within the same settings group/section context.
    QString dependsOn;

    // If non-zero, this Bool setting reads/writes a single bit of an
    // int-valued INI key. The widget displays as a checkbox; on save the
    // bit is set/cleared in the existing int and the full int is written
    // back. Used by PPSSPP for iShowStatusFlags. Default 0 = normal Bool.
    int bitmask = 0;
};
