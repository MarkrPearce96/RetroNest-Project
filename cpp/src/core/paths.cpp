#include "paths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>

QString Paths::s_root;

bool Paths::setRoot(const QString& rootPath) {
    if (rootPath.isEmpty()) {
        qWarning() << "[Paths] setRoot called with empty path";
        return false;
    }
    if (!QDir::isAbsolutePath(rootPath)) {
        qWarning() << "[Paths] setRoot requires an absolute path, got:" << rootPath;
        return false;
    }
    s_root = rootPath;
    return true;
}

QString Paths::root() {
    return s_root;
}

QString Paths::systemIdFor(const QString& emuId, const QStringList& systems) {
    return systems.isEmpty() ? emuId : systems.first();
}

QString Paths::emulatorsDir(const QString& emuId) {
    if (emuId.isEmpty()) return s_root + "/emulators";
    return s_root + "/emulators/" + emuId;
}

QString Paths::pcsx2ResourcesDir() {
    return s_root + "/emulators/libretro/cores/pcsx2_libretro_resources";
}

QString Paths::emulatorDataDir(const QString& emuId, const QString& systemId) {
    return s_root + "/emulators/" + emuId + "/" + systemId;
}

QString Paths::biosDir() {
    return s_root + "/bios";
}

QString Paths::romsDir(const QString& systemId) {
    if (systemId.isEmpty()) return s_root + "/roms";
    return s_root + "/roms/" + systemId;
}

QString Paths::mediaDir() {
    return s_root + "/downloaded_media";
}

QString Paths::mediaDir(const QString& system) {
    return s_root + "/downloaded_media/" + system;
}

QString Paths::mediaDir(const QString& system, const QString& mediaType) {
    return s_root + "/downloaded_media/" + system + "/" + mediaType;
}

QString Paths::configDir() {
    return s_root + "/config";
}

QString Paths::themesDir() {
    return s_root + "/themes";
}

void Paths::ensureDirectories() {
    QStringList dirs = {
        s_root,
        emulatorsDir(),
        biosDir(),
        romsDir(),
        mediaDir(),
        configDir(),
        themesDir(),
    };
    for (const auto& d : dirs) {
        if (!QDir().mkpath(d))
            qWarning() << "[Paths] Failed to create directory:" << d;
    }
    qInfo() << "[Paths] Root:" << s_root;
}

void Paths::ensureRomDirectories(const QStringList& systemIds) {
    QDir().mkpath(romsDir());
    for (const auto& sys : systemIds) {
        QDir().mkpath(romsDir(sys));
    }
}

QString Paths::appConfigPath() {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return appData + "/config.json";
}

// config.json holds independent keys (root, theme); every writer must
// read-modify-write so it can't clobber the others' values.
static QJsonObject readAppConfig() {
    QFile f(Paths::appConfigPath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

static void writeAppConfig(const QJsonObject& obj) {
    QFile f(Paths::appConfigPath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

QString Paths::loadSavedRoot() {
    return readAppConfig()["root"].toString();
}

void Paths::saveRoot(const QString& rootPath) {
    QJsonObject obj = readAppConfig();
    obj["root"] = rootPath;
    writeAppConfig(obj);
    qInfo() << "[Paths] Saved root to" << appConfigPath();
}

QString Paths::loadSavedTheme() {
    return readAppConfig()["theme"].toString();
}

void Paths::saveTheme(const QString& themeId) {
    QJsonObject obj = readAppConfig();
    obj["theme"] = themeId;
    writeAppConfig(obj);
}
