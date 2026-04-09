#include "scraper_credentials.h"
#include "core/paths.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>

QString ScraperCredentials::filePath() {
    return Paths::configDir() + "/scraper.json";
}

bool ScraperCredentials::load() {
    // Dev credentials injected at compile time via CMake (already quoted string literals)
    devId = QStringLiteral(SCREENSCRAPER_DEV_ID);
    devPassword = QStringLiteral(SCREENSCRAPER_DEV_PASSWORD);
    softname = QStringLiteral(SCREENSCRAPER_SOFTNAME);

    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    ssId = obj["ssid"].toString();
    ssPassword = obj["sspassword"].toString();

    return true;
}

bool ScraperCredentials::save() const {
    QJsonObject obj;
    obj["ssid"] = ssId;
    obj["sspassword"] = ssPassword;

    QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    f.write(QJsonDocument(obj).toJson());
    f.close();
    return true;
}

void ScraperCredentials::clearUser() {
    ssId.clear();
    ssPassword.clear();
    QFile::remove(filePath());
}
