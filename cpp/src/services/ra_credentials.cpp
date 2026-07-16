#include "ra_credentials.h"
#include "core/paths.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

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
    encoreMode    = obj["encoreMode"].toBool(false);

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
    obj["encoreMode"]    = encoreMode;

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
    encoreMode = false;
    QFile::remove(filePath());
}
