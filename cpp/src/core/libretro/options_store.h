#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <atomic>

struct CoreOption {
    QString key;
    QString label;
    QString defaultValue;
    QStringList values;
};

class OptionsStore {
public:
    bool load(const QString& jsonPath, const QVector<CoreOption>& coreOptions);
    bool save() const;
    QString get(const QString& key) const;
    void set(const QString& key, const QString& value);
    bool consumeDirty();

private:
    QString m_path;
    QHash<QString, QString> m_values;
    std::atomic<bool> m_dirty{false};
};
