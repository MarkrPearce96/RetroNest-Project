#pragma once

#include <QString>
#include <QStringList>

struct EmulatorManifest;

/**
 * Paths — derives all managed directory paths from a single root.
 * Call setRoot() once at startup before using any other method.
 */
class Paths {
public:
    /** Set the root directory. Returns false if path is empty or not absolute. */
    static bool setRoot(const QString& rootPath);
    static QString root();

    /** Derive the primary system ID from a manifest. */
    static QString systemIdFor(const QString& emuId, const QStringList& systems);

    static QString emulatorsDir(const QString& emuId = {});
    static QString dataDir(const QString& emuId = {});
    static QString biosDir();
    static QString savesDir(const QString& systemId = {});
    static QString cacheDir(const QString& systemId = {});
    static QString romsDir(const QString& systemId = {});
    /** Media directory for scraped content (ES-DE style). */
    static QString mediaDir();
    static QString mediaDir(const QString& system);
    static QString mediaDir(const QString& system, const QString& mediaType);
    static QString configDir();
    static QString themesDir();

    /** Create the base directory structure under root. */
    static void ensureDirectories();

    /** Create per-system ROM subdirectories. */
    static void ensureRomDirectories(const QStringList& systemIds);

    /** Path to the app-level config file (stores root location). */
    static QString appConfigPath();

    /** Load previously saved root path. Returns empty if none saved. */
    static QString loadSavedRoot();

    /** Save the root path for next launch. */
    static void saveRoot(const QString& rootPath);

private:
    static QString s_root;
};
