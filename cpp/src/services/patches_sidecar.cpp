#include "patches_sidecar.h"

#include <QFile>
#include <QTextStream>

std::optional<PatchesSidecar> PatchesSidecar::read(const QString& path) {
    QFile f(path);
    if (!f.exists()) return std::nullopt;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return std::nullopt;

    PatchesSidecar out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;  // skip "no equals" and "=value-without-key"
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();
        if (key == "tag")               out.tag = val;
        else if (key == "published_at") out.publishedAt = val;
        else if (key == "installed_at") out.installedAt = val;
        else if (key == "sha256")       out.sha256 = val;
        // unknown keys silently ignored (forward-compat)
    }
    return out;
}

bool PatchesSidecar::write(const QString& path, const PatchesSidecar& s) {
    const QString tmp = path + ".tmp";
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return false;
        QTextStream out(&f);
        out << "tag=" << s.tag << "\n"
            << "published_at=" << s.publishedAt << "\n"
            << "installed_at=" << s.installedAt << "\n"
            << "sha256=" << s.sha256 << "\n";
    }
    // QFile::rename refuses to overwrite an existing destination on some
    // platforms; remove first.
    QFile::remove(path);
    return QFile::rename(tmp, path);
}
