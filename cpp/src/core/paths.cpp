#include "paths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>

QString Paths::s_root;
QString Paths::s_romsRoot;
QString Paths::s_biosRoot;

bool Paths::setRoot(const QString& rootPath) {
    // Normalize so a stray doubled slash (e.g. a legacy wizard save of
    // ".../Documents//RetroNest") can't propagate into every derived path.
    const QString clean = QDir::cleanPath(rootPath);
    if (clean.isEmpty()) {
        qWarning() << "[Paths] setRoot called with empty path";
        return false;
    }
    if (!QDir::isAbsolutePath(clean)) {
        qWarning() << "[Paths] setRoot requires an absolute path, got:" << rootPath;
        return false;
    }
    s_root = clean;
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

void Paths::setRomsRoot(const QString& path) {
    s_romsRoot = path.isEmpty() ? QString() : QDir::cleanPath(path);
}
void Paths::setBiosRoot(const QString& path) {
    s_biosRoot = path.isEmpty() ? QString() : QDir::cleanPath(path);
}
QString Paths::romsRoot() {
    return s_romsRoot.isEmpty() ? (s_root + "/roms") : s_romsRoot;
}
QString Paths::biosRoot() {
    return s_biosRoot.isEmpty() ? (s_root + "/bios") : s_biosRoot;
}

QString Paths::biosDir() {
    return biosRoot();
}

QString Paths::romsDir(const QString& systemId) {
    if (systemId.isEmpty()) return romsRoot();
    return romsRoot() + "/" + systemId;
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
    const QString raw = readAppConfig()["root"].toString();
    if (raw.isEmpty())
        return raw;
    const QString clean = QDir::cleanPath(raw);
    // Heal a legacy dirty value in place so the persisted config stops
    // carrying the doubled slash forward on every launch.
    if (clean != raw)
        saveRoot(clean);
    return clean;
}

void Paths::saveRoot(const QString& rootPath) {
    QJsonObject obj = readAppConfig();
    obj["root"] = QDir::cleanPath(rootPath);
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

QString Paths::loadSavedRomsRoot() { return readAppConfig()["romsRoot"].toString(); }
void Paths::saveRomsRoot(const QString& path) {
    QJsonObject obj = readAppConfig();
    obj["romsRoot"] = path.isEmpty() ? QString() : QDir::cleanPath(path);
    writeAppConfig(obj);
}
QString Paths::loadSavedBiosRoot() { return readAppConfig()["biosRoot"].toString(); }
void Paths::saveBiosRoot(const QString& path) {
    QJsonObject obj = readAppConfig();
    obj["biosRoot"] = path.isEmpty() ? QString() : QDir::cleanPath(path);
    writeAppConfig(obj);
}
