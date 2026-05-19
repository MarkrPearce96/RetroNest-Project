// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "path_overrides_store.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QStandardPaths>

PathOverridesStore::PathOverridesStore(const QString& filePath)
    : m_filePath(filePath) {
    load();
}

PathOverridesStore& PathOverridesStore::instance() {
    static const QString defaultPath = []() {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        return dir + "/path_overrides.json";
    }();
    static PathOverridesStore singleton(defaultPath);
    return singleton;
}

void PathOverridesStore::load() {
    QMutexLocker lock(&m_mutex);
    m_root = {};
    QFile f(m_filePath);
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return;  // corrupt → empty
    if (!doc.isObject()) return;
    m_root = doc.object();
}

bool PathOverridesStore::save() const {
    QFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray data = QJsonDocument(m_root).toJson(QJsonDocument::Indented);
    return f.write(data) == static_cast<qint64>(data.size());
}

QString PathOverridesStore::read(const QString& emuId, const QString& key) const {
    QMutexLocker lock(&m_mutex);
    const auto emu = m_root.value(emuId).toObject();
    const auto v = emu.value(key).toString();
    return v.isEmpty() ? QString() : v;
}

void PathOverridesStore::write(const QString& emuId, const QString& key,
                               const QString& path) {
    QMutexLocker lock(&m_mutex);
    auto emu = m_root.value(emuId).toObject();
    if (path.isEmpty())
        emu.remove(key);
    else
        emu.insert(key, path);
    if (emu.isEmpty())
        m_root.remove(emuId);
    else
        m_root.insert(emuId, emu);
    if (!save())
        qWarning() << "[PathOverridesStore] Failed to write" << m_filePath;
}

void PathOverridesStore::clear(const QString& emuId, const QString& key) {
    write(emuId, key, QString());
}
