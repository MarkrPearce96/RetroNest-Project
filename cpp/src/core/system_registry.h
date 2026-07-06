#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

/**
 * SystemRegistry — single source for per-system facts, loaded once at
 * startup from manifests/systems.json (packet 7 stage 3). Replaces the
 * four maps that used to drift independently: ThemeContext display names,
 * Scraper ScreenScraper IDs, RAClient's console map, and the per-adapter
 * raConsoleId() overrides.
 *
 * Static holder in the Paths mold: load() once in main(), then pure
 * lookups anywhere. Lookups lowercase the key (matching the historical
 * .toLower() behavior of the maps this replaces).
 *
 * Fallback contracts (consumers rely on these):
 *   displayName(unknown)     -> the raw systemId unchanged
 *   screenScraperId(unknown) -> -1
 *   raConsoleId(unknown or system without RA support) -> -1
 */
class SystemRegistry {
public:
    /** Load from a systems.json path. Returns false + qWarning on error;
     *  the previously loaded table (if any) is left untouched. */
    static bool load(const QString& jsonPath);

    /** Parse from raw JSON bytes — load() delegates here; also the test
     *  seam. Same failure contract as load(). */
    static bool loadFromData(const QByteArray& json);

    static bool isLoaded();

    static QString displayName(const QString& systemId);
    static int screenScraperId(const QString& systemId);
    static int raConsoleId(const QString& systemId);

    /** Distinct RA console ids across every system that declares one —
     *  drives RAService's achievement-catalog fetch set. */
    static QList<int> allRaConsoleIds();

private:
    struct Entry {
        QString name;
        int ssId = -1;
        int raId = -1;
    };
    static QHash<QString, Entry> s_entries;
};
