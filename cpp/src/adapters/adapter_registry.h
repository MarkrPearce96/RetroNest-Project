#pragma once

#include "emulator_adapter.h"
#include "core/manifest_loader.h"
#include <QString>
#include <QStringList>
#include <QHash>
#include <unordered_map>
#include <memory>

/**
 * AdapterRegistry — maps manifest IDs to EmulatorAdapter instances.
 * Singleton. Call registerBuiltinAdapters() once at startup.
 *
 * Uses std::unordered_map because QHash requires copyable value types,
 * and std::unique_ptr is move-only.
 */
class AdapterRegistry {
public:
    static AdapterRegistry& instance();

    /** Register the built-in adapters (pcsx2, duckstation). */
    void registerBuiltinAdapters();

    /** Register a custom adapter for a given manifest id. */
    void registerAdapter(const QString& id, std::unique_ptr<EmulatorAdapter> adapter);

    /** Look up an adapter by manifest id. Returns nullptr if none registered. */
    EmulatorAdapter* adapterFor(const QString& id) const;

    /** Returns list of manifest IDs that have no registered adapter. */
    QStringList validateManifests(const ManifestLoader& loader) const;

private:
    AdapterRegistry() = default;
    struct QStringHash {
        size_t operator()(const QString& s) const { return qHash(s); }
    };
    std::unordered_map<QString, std::unique_ptr<EmulatorAdapter>, QStringHash> m_adapters;
};
