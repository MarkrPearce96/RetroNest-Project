// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// PathOverridesStore — JSON-backed persistence for per-emulator
// directory overrides exposed in the Paths settings UI.
//
// Used as the backend when adapter->configFilePath().isEmpty()
// (i.e., libretro adapters). Native adapters keep their existing
// INI-based persistence via ConfigService.

#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QString>

class PathOverridesStore {
public:
    // Constructor used in tests with an explicit file path.
    explicit PathOverridesStore(const QString& filePath);

    // Singleton access for production code. Resolves to
    // <writable-app-data>/path_overrides.json — typically
    // ~/Library/Application Support/RetroNest/path_overrides.json
    // on macOS.
    static PathOverridesStore& instance();

    // Returns the override for (emuId, key), or empty string if
    // unset / file missing / file malformed. Empty-string values
    // on disk are treated as unset (fall back to default).
    QString read(const QString& emuId, const QString& key) const;

    // Writes the override and persists to disk immediately.
    // Empty path removes the override (equivalent to clear()).
    void write(const QString& emuId, const QString& key, const QString& path);

    // Removes the override for (emuId, key). No-op if unset.
    void clear(const QString& emuId, const QString& key);

private:
    void load();
    // `save()` is `const` because it only reads m_root under the mutex;
    // m_root is `mutable` so it can also be written from non-const
    // member functions (read/write helpers).
    bool save() const;

    QString          m_filePath;
    mutable QMutex   m_mutex;        // guards m_root + file I/O
    mutable QJsonObject m_root;      // {emuId: {key: path, ...}, ...}
};
