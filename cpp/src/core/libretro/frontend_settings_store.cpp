#include "frontend_settings_store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

FrontendSettingsStore::FrontendSettingsStore(QObject* parent)
    : QObject(parent) {}

bool FrontendSettingsStore::load(const QString& jsonPath,
                                  const QVector<QPair<QString, QString>>& defaults) {
    QWriteLocker lk(&m_lock);
    m_path = jsonPath;
    m_values.clear();

    if (jsonPath == ":memory:") {
        m_path.clear();   // sentinel: never write to disk
        for (const auto& pair : defaults)
            m_values.insert(pair.first, pair.second);
        return true;
    }

    // Read existing persisted values (if any).
    QHash<QString, QString> existing;
    QFile f(jsonPath);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(f.readAll());
        const auto obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it)
            existing.insert(it.key(), it.value().toString());
        f.close();
    }

    // Merge: keep persisted values; fall back to default for any missing key.
    for (const auto& pair : defaults) {
        auto it = existing.constFind(pair.first);
        if (it != existing.constEnd())
            m_values.insert(pair.first, it.value());
        else
            m_values.insert(pair.first, pair.second);
    }

    lk.unlock();
    return save();
}

bool FrontendSettingsStore::save() const {
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

QString FrontendSettingsStore::get(const QString& key) const {
    QReadLocker lk(&m_lock);
    return m_values.value(key);
}

void FrontendSettingsStore::set(const QString& key, const QString& value) {
    {
        QWriteLocker lk(&m_lock);
        if (!m_values.contains(key)) {
            qWarning() << "[FrontendSettingsStore] set: unknown key" << key;
            return;
        }
        if (m_values.value(key) == value) return;
        m_values.insert(key, value);
        m_dirty.store(true);
    }
    save();   // re-locks as read, that's fine
    emit frontendSettingChanged(key, value);
}

bool FrontendSettingsStore::consumeDirty() {
    return m_dirty.exchange(false);
}
