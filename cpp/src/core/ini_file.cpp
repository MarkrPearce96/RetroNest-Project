#include "ini_file.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>

bool IniFile::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    m_lines.clear();
    QString currentSection;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString raw = in.readLine();
        QString trimmed = raw.trimmed();

        Line line;
        line.raw = raw;

        if (trimmed.isEmpty()) {
            line.type = Line::Blank;
        } else if (trimmed.startsWith(';') || trimmed.startsWith('#')) {
            line.type = Line::Comment;
        } else if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            line.type = Line::Section;
            currentSection = trimmed.mid(1, trimmed.length() - 2);
            line.section = currentSection;
        } else {
            int eq = trimmed.indexOf('=');
            if (eq > 0) {
                line.type = Line::KeyValue;
                line.section = currentSection;
                line.key = trimmed.left(eq).trimmed();
                line.value = trimmed.mid(eq + 1).trimmed();
            } else {
                line.type = Line::Comment; // malformed, treat as comment
            }
        }

        m_lines.append(line);
    }

    rebuildIndex();
    return true;
}

QString IniFile::serialize() const {
    QString out;
    QTextStream ts(&out);
    for (const auto& line : m_lines) {
        switch (line.type) {
        case Line::Section:
            ts << "[" << line.section << "]\n";
            break;
        case Line::KeyValue:
            ts << line.key << " = " << line.value << "\n";
            break;
        default:
            ts << line.raw << "\n";
            break;
        }
    }
    ts.flush();
    return out;
}

bool IniFile::save(const QString& path) {
    // QSaveFile: writes into <path>.<random> then renames atomically on
    // commit(). A torn write (process killed mid-flush, disk full) leaves
    // the original file untouched. Replaces the previous direct-write
    // path that could leave half-written INI on disk.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray bytes = serialize().toUtf8();
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

QString IniFile::value(const QString& section, const QString& key, const QString& defaultValue) const {
    int idx = findKey(section, key);
    if (idx >= 0) return m_lines[idx].value;
    return defaultValue;
}

void IniFile::setValue(const QString& section, const QString& key, const QString& val) {
    int idx = findKey(section, key);
    if (idx >= 0) {
        m_lines[idx].value = val;
        return;
    }

    // Key not found — find or create section, then append key
    int secIdx = findSection(section);
    bool sectionAppended = false;
    if (secIdx < 0) {
        // Add blank line + section at end
        Line blank;
        blank.type = Line::Blank;
        m_lines.append(blank);

        Line secLine;
        secLine.type = Line::Section;
        secLine.section = section;
        m_lines.append(secLine);
        secIdx = m_lines.size() - 1;
        sectionAppended = true;
    }

    // Insert key after last line in this section
    int insertAt = secIdx + 1;
    while (insertAt < m_lines.size()) {
        if (m_lines[insertAt].type == Line::Section) break;
        insertAt++;
    }

    Line kvLine;
    kvLine.type = Line::KeyValue;
    kvLine.section = section;
    kvLine.key = key;
    kvLine.value = val;
    m_lines.insert(insertAt, kvLine);

    // Incremental index update — much cheaper than rebuilding both hashes
    // from scratch on every new key. If insertAt is at the end (sectionAppended
    // path), nothing in the hash points past it so no shift is needed.
    if (!sectionAppended) {
        for (auto it = m_sectionIndex.begin(); it != m_sectionIndex.end(); ++it)
            if (it.value() >= insertAt) ++it.value();
        for (auto it = m_keyIndex.begin(); it != m_keyIndex.end(); ++it)
            if (it.value() >= insertAt) ++it.value();
    } else {
        m_sectionIndex.insert(section, secIdx);
    }
    m_keyIndex.insert(keyIndexKey(section, key), insertAt);
}

bool IniFile::containsKey(const QString& section, const QString& key) const {
    return findKey(section, key) >= 0;
}

QStringList IniFile::keys(const QString& section) const {
    QStringList result;
    for (const auto& line : m_lines) {
        if (line.type == Line::KeyValue && line.section == section)
            result.append(line.key);
    }
    return result;
}

void IniFile::rebuildIndex() {
    m_sectionIndex.clear();
    m_keyIndex.clear();
    for (int i = 0; i < m_lines.size(); i++) {
        if (m_lines[i].type == Line::Section)
            m_sectionIndex.insert(m_lines[i].section, i);
        else if (m_lines[i].type == Line::KeyValue)
            m_keyIndex.insert(keyIndexKey(m_lines[i].section, m_lines[i].key), i);
    }
}

int IniFile::findSection(const QString& section) const {
    return m_sectionIndex.value(section, -1);
}

int IniFile::findKey(const QString& section, const QString& key) const {
    return m_keyIndex.value(keyIndexKey(section, key), -1);
}
