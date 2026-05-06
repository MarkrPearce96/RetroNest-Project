#include "options_store.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

bool OptionsStore::load(const QString& jsonPath, const QVector<CoreOption>& coreOptions) {
    m_path = jsonPath;
    m_values.clear();

    QHash<QString, QString> existing;
    QFile f(jsonPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            existing.insert(it.key(), it.value().toString());
        f.close();
    }

    for (const auto& opt : coreOptions) {
        auto it = existing.constFind(opt.key);
        if (it != existing.constEnd() && opt.values.contains(it.value()))
            m_values.insert(opt.key, it.value());
        else
            m_values.insert(opt.key, opt.defaultValue);
    }
    return save();
}

bool OptionsStore::save() const {
    if (m_path.isEmpty()) return false;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QJsonObject obj;
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it)
        obj.insert(it.key(), it.value());
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

QString OptionsStore::get(const QString& key) const {
    return m_values.value(key);
}

void OptionsStore::set(const QString& key, const QString& value) {
    if (!m_values.contains(key) || m_values.value(key) == value) return;
    m_values.insert(key, value);
    m_dirty.store(true);
    save();
}

bool OptionsStore::consumeDirty() {
    return m_dirty.exchange(false);
}
