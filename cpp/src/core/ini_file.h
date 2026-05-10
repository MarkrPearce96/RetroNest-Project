#pragma once

#include <QHash>
#include <QString>

/**
 * IniFile — simple INI reader/writer that preserves structure.
 * Reads section/key pairs, allows getting/setting values, writes back.
 * Uses a hash index for O(1) lookups while preserving line order for round-trip.
 */
class IniFile {
public:
    bool load(const QString& path);
    /** Atomic save: writes to a temporary file and renames over `path` only
     *  on a clean flush. A torn write (process killed mid-flush) leaves the
     *  original file untouched instead of half-overwritten. */
    bool save(const QString& path);
    /** Render the in-memory state to a QString in the same byte-for-byte form
     *  save() would write. Used by callers that need to do their own
     *  multi-file two-phase commit. */
    QString serialize() const;

    QString value(const QString& section, const QString& key, const QString& defaultValue = {}) const;
    void setValue(const QString& section, const QString& key, const QString& value);
    bool containsKey(const QString& section, const QString& key) const;
    QStringList keys(const QString& section) const;

private:
    // Ordered list of lines for round-trip preservation
    struct Line {
        enum Type { Comment, Section, KeyValue, Blank };
        Type type = Blank;
        QString section;   // for KeyValue lines
        QString key;
        QString value;
        QString raw;       // original text for comments/blanks
    };
    QVector<Line> m_lines;

    // Hash index: "section\0key" → line index for O(1) lookups
    QHash<QString, int> m_sectionIndex;  // section → line index
    QHash<QString, int> m_keyIndex;      // "section\0key" → line index

    static QString keyIndexKey(const QString& section, const QString& key) {
        return section + QChar('\0') + key;
    }

    void rebuildIndex();
    int findSection(const QString& section) const;
    int findKey(const QString& section, const QString& key) const;
};
