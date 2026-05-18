#pragma once

#include <QByteArray>
#include <QString>

namespace InstallerUtils {

/** Synchronous HTTP GET. Returns body on success, empty on failure.
 *  Times out after `timeoutMs` (default 30s). Logs failures with the
 *  given `context` prefix (e.g. "[Installer]", "[Patches]"). */
QByteArray httpGet(const QString& url, int timeoutMs = 30000,
                   const QString& context = QStringLiteral("[InstallerUtils]"));

/** Lower-case hex SHA256 of the file at `path`. Empty on read failure. */
QString computeSha256(const QString& path);

/** True if `expected` is empty (skip verify) or matches the file's SHA256.
 *  Failures are logged with the given `context` prefix. */
bool verifySha256(const QString& path, const QString& expected,
                  const QString& context = QStringLiteral("[InstallerUtils]"));

} // namespace InstallerUtils
