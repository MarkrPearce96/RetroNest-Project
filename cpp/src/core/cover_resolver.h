#pragma once

#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace CoverResolver {

/**
 * Resolve the cover image path for a game, checking:
 *   1. Database cover_path (if it exists on disk)
 *   2. {mediaDir}/{system}/covers/{title}.{ext}
 *   3. {mediaDir}/{system}/covers/{romBaseName}.{ext}
 * Returns empty string if no cover found.
 */
inline QString resolve(const QString& coverPath, const QString& mediaDir,
                       const QString& system, const QString& title,
                       const QString& romPath) {
    if (!coverPath.isEmpty() && QFileInfo::exists(coverPath))
        return coverPath;

    if (mediaDir.isEmpty()) return {};

    static const QStringList exts = {"png", "jpg", "jpeg", "webp"};

    for (const auto& ext : exts) {
        QString path = mediaDir + "/" + system + "/covers/" + title + "." + ext;
        if (QFileInfo::exists(path)) return path;
    }

    QString romBase = QFileInfo(romPath).completeBaseName();
    for (const auto& ext : exts) {
        QString path = mediaDir + "/" + system + "/covers/" + romBase + "." + ext;
        if (QFileInfo::exists(path)) return path;
    }

    return {};
}

} // namespace CoverResolver
