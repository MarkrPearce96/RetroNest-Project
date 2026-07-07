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

    /** Resource dir consumed by the PCSX2 libretro core
     *  (`<emulators>/libretro/cores/pcsx2_libretro_resources`). Where
     *  patches.zip, Roboto-Regular.ttf, and similar dladdr-resolved
     *  bundle assets live. */
    static QString pcsx2ResourcesDir();

    /**
     * Single per-emulator, per-system data root. Every runtime folder an
     * emulator needs (savestates, memcards, screenshots, cache, cheats,
     * textures, videos, logs, patches, …) lives as a subdirectory of this
     * path. Returns {root}/emulators/{emuId}/{systemId}/.
     *
     * This is the modern replacement for the split savesDir()/dataDir()
     * layout. PPSSPP's forced {memstick}/PSP/<subdir> scheme is the
     * structural model: every emulator now follows the same
     * "everything under emulators/{emuId}/{systemId}/" pattern.
     */
    static QString emulatorDataDir(const QString& emuId, const QString& systemId);

    static QString biosDir();
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

    /** Load previously saved theme id. Returns empty if none saved. */
    static QString loadSavedTheme();

    /** Save the chosen theme id for next launch. */
    static void saveTheme(const QString& themeId);

private:
    static QString s_root;
};
