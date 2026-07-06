#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/** One controller-mapping button on the emulator detail page (packet 7
 *  stage 3 `detail_page.controller_pages`). `type` is the adapter
 *  controllerTypes() id passed to showControllerMapping; empty = the
 *  adapter's default page. */
struct ManifestControllerPage {
    QString label;   // button text, e.g. "GameCube Controller"
    QString type;    // e.g. "GCPad1"; "" = default
};

/**
 * EmulatorManifest — pure data loaded from manifests JSON files.
 * No behavior, no platform branching. Adapters handle all logic.
 */
struct EmulatorManifest {
    QString id;
    QString name;
    QString description;
    QStringList systems;
    QString github_repo;
    QString executable;
    QString install_folder;
    QStringList rom_extensions;
    QStringList launch_args;  // may contain {rom_path} placeholder
    QString backend = "process";   // "process" (default) | "libretro"
    QString core_dylib;             // libretro: filename of the .dylib (relative to cores/)
    QString core_buildbot_path;     // libretro: appended to buildbot URL prefix
    QString core_arch;              // libretro: "universal" | "x86_64" | "arm64" | "" (undeclared)

    // Packet 7 stage 3.
    int manifest_version = 0;       // 0 = pre-versioning file (loader warns)
    QString logo;                   // qrc path for tiles/popups ("" = none)
    QVector<ManifestControllerPage> controller_pages;  // empty → one default "Controller Mapping"
    bool has_patches = false;       // detail-page "Refresh <Name> Patches" action
};
