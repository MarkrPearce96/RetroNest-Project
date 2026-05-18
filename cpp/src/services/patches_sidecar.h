#pragma once

#include <QString>
#include <optional>

struct PatchesSidecar {
    QString tag;          // GitHub release tag_name, e.g. "v2026.04.15"
    QString publishedAt;  // GitHub published_at ISO timestamp
    QString installedAt;  // local install time (Qt::ISODateWithMs)
    QString sha256;       // lower-case hex; empty if upstream gave no digest

    /** Read `path` and parse key=value lines. Tolerates missing keys,
     *  empty files, malformed lines. Returns nullopt only if the file
     *  cannot be opened for reading. */
    static std::optional<PatchesSidecar> read(const QString& path);

    /** Atomic write: writes to `path + ".tmp"` then renames. Returns false
     *  on any I/O error. */
    static bool write(const QString& path, const PatchesSidecar& s);
};
