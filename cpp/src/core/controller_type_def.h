#pragma once

#include <QHash>
#include <QString>

/**
 * ControllerTypeDef — describes an available controller type for an emulator.
 *
 * `slotTitleOverrides` lets an emulator override the per-slot title rendered
 * by ControllerBindingsView (e.g. the Wii Remote shows "TILT" instead of
 * "LEFT ANALOG" above its tilt-axis bindings). Empty map → use the view's
 * built-in titles.
 */
struct ControllerTypeDef {
    QString id;
    QString displayName;
    QString svgResource;
    QHash<QString, QString> slotTitleOverrides = {};
};
