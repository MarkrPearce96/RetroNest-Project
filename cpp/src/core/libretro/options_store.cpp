#include "options_store.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

bool OptionsStore::load(const QString& jsonPath, const QVector<CoreOption>& coreOptions) {
    QWriteLocker lk(&m_lock);
    m_path = jsonPath;
    m_values.clear();

    if (jsonPath == ":memory:") {
        m_path.clear();          // sentinel: never write to disk
        for (const auto& opt : coreOptions) m_values.insert(opt.key, opt.defaultValue);
        return true;
    }

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
    lk.unlock();
    return save();
}

bool OptionsStore::save() const {
    QReadLocker lk(&m_lock);
    if (m_path.isEmpty()) return true;   // :memory: mode — pretend success
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QJsonObject obj;
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it)
        obj.insert(it.key(), it.value());
    lk.unlock();
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

QString OptionsStore::get(const QString& key) const {
    QReadLocker lk(&m_lock);
    return m_values.value(key);
}

void OptionsStore::set(const QString& key, const QString& value) {
    {
        QWriteLocker lk(&m_lock);
        if (!m_values.contains(key)) {
            qWarning() << "[OptionsStore] set: unknown key" << key;
            return;
        }
        if (m_values.value(key) == value) return;
        m_values.insert(key, value);
        m_dirty.store(true);
    }
    save();   // re-locks as read, that's fine
}

bool OptionsStore::consumeDirty() {
    return m_dirty.exchange(false);
}
