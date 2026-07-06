#include "system_registry.h"

#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

QHash<QString, SystemRegistry::Entry> SystemRegistry::s_entries;

bool SystemRegistry::load(const QString& jsonPath) {
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[SystemRegistry] cannot open" << jsonPath;
        return false;
    }
    return loadFromData(f.readAll());
}

bool SystemRegistry::loadFromData(const QByteArray& json) {
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[SystemRegistry] parse error:" << err.errorString();
        return false;
    }
    const QJsonObject systems = doc.object().value("systems").toObject();
    if (systems.isEmpty()) {
        qWarning() << "[SystemRegistry] no systems object in registry JSON";
        return false;
    }
    QHash<QString, Entry> entries;
    for (auto it = systems.begin(); it != systems.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        Entry e;
        e.name = o.value("name").toString();
        e.ssId = o.value("screenscraper_id").toInt(-1);
        e.raId = o.value("ra_console_id").toInt(-1);
        entries.insert(it.key().toLower(), e);
    }
    s_entries = std::move(entries);
    return true;
}

bool SystemRegistry::isLoaded() {
    return !s_entries.isEmpty();
}

QString SystemRegistry::displayName(const QString& systemId) {
    const auto it = s_entries.constFind(systemId.toLower());
    return (it != s_entries.constEnd() && !it->name.isEmpty()) ? it->name : systemId;
}

int SystemRegistry::screenScraperId(const QString& systemId) {
    return s_entries.value(systemId.toLower()).ssId;
}

int SystemRegistry::raConsoleId(const QString& systemId) {
    return s_entries.value(systemId.toLower()).raId;
}

QList<int> SystemRegistry::allRaConsoleIds() {
    QSet<int> ids;
    for (const auto& e : s_entries) {
        if (e.raId > 0)
            ids.insert(e.raId);
    }
    return QList<int>(ids.begin(), ids.end());
}
