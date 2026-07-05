#pragma once
// Packet 7 Stage 2: per-core curation overlay.
//
// The core's declared option table (DeclaredOptionsDoc) is the source of
// truth for keys, value sets, defaults, and wording. An adapter's overlay
// curates PRESENTATION: which options appear, under which UI categories,
// in what order, with what widget type — plus the rare deliberate override
// (a better label, a RetroNest-specific default).
//
// Rules (LibretroAdapter::settingsSchema() merge):
//  - Overlay order = row order. One overlay entry per curated key; listing
//    N categories yields N rows sharing the key (the "Recommended"
//    cross-listing pattern) — edits persist to the same OptionsStore key.
//  - Declared options with no overlay entry are NOT rendered (still valid
//    in OptionsStore; new upstream options arrive hidden until curated).
//  - Overlay keys missing from the declared table are skipped with a
//    warning (schema self-heals when a core drops an option).
//  - Empty override fields mean "adopt the core's wording/default"
//    (Packet 7 decision 4).

#include <QString>
#include <QStringList>

#include "setting_def.h"

struct OptionOverlay {
    QString key;              // core option key this entry curates
    QStringList categories;   // UI categories to list under (>=1)
    QString subcategory;      // optional sub-tab routing
    QString group;            // optional group-box title

    QString labelOverride;    // empty = core's desc
    QString tooltipOverride;  // empty = core's info
    QString defaultOverride;  // empty = core's default (deliberate RetroNest defaults only)

    // Widget shaping. hasTypeOverride=false renders as Combo (values from
    // the declared table). Set true + typeOverride for Bool/Int/Float/slider
    // rows carried over from the hand schemas.
    bool hasTypeOverride = false;
    SettingDef::Type typeOverride = SettingDef::Combo;
    double minVal = 0, maxVal = 100, step = 1;
    QString layout;           // "paired" / "slider"
    QString suffix;           // e.g. "ms", "%"

    QString dependsOn;        // gate expression (see setting_def.h)
    QString recommendedValue; // description-bar hint
};
