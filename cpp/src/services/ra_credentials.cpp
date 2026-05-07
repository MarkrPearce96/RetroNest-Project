#include "ra_credentials.h"
#include "core/paths.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

QString RACredentials::filePath() {
    return Paths::configDir() + "/retroachievements.json";
}

bool RACredentials::load() {
    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    username   = obj["username"].toString();
    apiKey     = obj["apiKey"].toString();
    loginToken = obj["loginToken"].toString();

    hardcoreMode  = obj["hardcoreMode"].toBool(false);
    notifications = obj["notifications"].toBool(true);
    soundEffects  = obj["soundEffects"].toBool(true);

    promptedEmulators.clear();
    QJsonArray prompted = obj["promptedEmulators"].toArray();
    for (const auto& val : prompted)
        promptedEmulators.append(val.toString());

    return true;
}

bool RACredentials::save() const {
    QJsonObject obj;
    obj["username"]   = username;
    obj["apiKey"]     = apiKey;
    obj["loginToken"] = loginToken;

    obj["hardcoreMode"]  = hardcoreMode;
    obj["notifications"] = notifications;
    obj["soundEffects"]  = soundEffects;

    QJsonArray prompted;
    for (const auto& emu : promptedEmulators)
        prompted.append(emu);
    obj["promptedEmulators"] = prompted;

    QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    f.write(QJsonDocument(obj).toJson());
    f.close();
    return true;
}

void RACredentials::clearUser() {
    username.clear();
    apiKey.clear();
    loginToken.clear();
    hardcoreMode = false;
    notifications = true;
    soundEffects = true;
    promptedEmulators.clear();
    QFile::remove(filePath());
}
