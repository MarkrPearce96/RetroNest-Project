#pragma once

#include <QString>
#include <QStringList>

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
};
