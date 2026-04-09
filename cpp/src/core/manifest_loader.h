#pragma once

#include "manifest.h"
#include <QHash>
#include <QVector>
#include <QString>

/**
 * ManifestLoader — scans a directory of JSON manifest files and provides
 * lookup by ID or iteration over all loaded emulators.
 */
class ManifestLoader {
public:
    /**
     * Load all *.json files from the given directory.
     * Returns true if at least one manifest was loaded successfully.
     * Logs warnings for files that fail validation.
     */
    bool loadAll(const QString& manifestsDir);

    /** Look up a manifest by its id. Returns nullptr if not found. */
    const EmulatorManifest* emulatorById(const QString& id) const;

    /** All successfully loaded manifests. */
    const QVector<EmulatorManifest>& allEmulators() const;

    /** Test helper — inject a manifest directly without loading from disk. */
    void injectManifest(const EmulatorManifest& m) {
        m_idIndex.insert(m.id, m_manifests.size());
        m_manifests.append(m);
    }

private:
    bool validateManifest(const EmulatorManifest& m, const QString& filePath) const;

    QVector<EmulatorManifest> m_manifests;
    QHash<QString, int> m_idIndex;  // id → index into m_manifests
};
