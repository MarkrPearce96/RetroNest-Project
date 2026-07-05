#pragma once
// Packet 7 Stage 2: full-metadata capture of a core's declared option table.
//
// The core's SET_CORE_OPTIONS_V2/_INTL payload is the single source of
// settings-schema truth. This doc captures EVERYTHING the core declares
// (labels, per-value labels, category routing, info text) — unlike the thin
// CoreOption view, which OptionsStore uses only for value validation.
//
// Persisted as `declared_options.json` beside each core's options.json:
// written by CoreRuntime on every session start (core just declared them),
// seeded offline by CoreProber when no session has ever run. The settings
// UI renders from this doc merged with the adapter's curation overlay.

#include <QJsonDocument>
#include <QString>
#include <QVector>
#include <optional>

#include "options_store.h"   // CoreOption (thin view)

struct retro_core_options_v2;

struct DeclaredOptionValue {
    QString value;
    QString label;   // empty when the core declared no display label
};

struct DeclaredOption {
    QString key;
    QString label;         // desc_categorized when present, else desc
    QString categoryKey;   // core's own category routing ("" = uncategorized)
    QString info;          // info_categorized when present, else info
    QString defaultValue;
    QVector<DeclaredOptionValue> values;
};

struct DeclaredCategory {
    QString key;
    QString label;
    QString info;
};

struct DeclaredOptionsDoc {
    int format = 1;
    QString coreLibraryVersion;   // retro_get_system_info().library_version
    QVector<DeclaredCategory> categories;
    QVector<DeclaredOption> options;

    bool isEmpty() const { return options.isEmpty(); }

    QJsonDocument toJson() const;
    static std::optional<DeclaredOptionsDoc> fromJson(const QByteArray& bytes);

    /** Atomic write via QSaveFile (indented). Returns false on I/O failure. */
    bool save(const QString& path) const;
    /** Missing/corrupt file → nullopt (callers fall back to CoreProber). */
    static std::optional<DeclaredOptionsDoc> load(const QString& path);

    /** Thin view for OptionsStore value-validation (labels dropped). */
    QVector<CoreOption> toCoreOptions() const;
};

/** Fill categories+options from a SET_CORE_OPTIONS_V2 payload (nullptr-safe;
 *  clears the doc's existing categories/options first). For V2_INTL, pass
 *  the payload's `us` member. */
void populateFromV2(DeclaredOptionsDoc& doc, const retro_core_options_v2* v2);
