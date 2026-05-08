#pragma once

#include <QHash>
#include <QString>

/**
 * ControllerTypeDef — describes an available controller type for an emulator.
 *
 * `slotTitleOverrides` lets an emulator override the per-slot title rendered
 * by ControllerBindingsView. Empty map → use the view's built-in titles
 * ("LEFT ANALOG", "SHOULDERS", etc.). Useful when a controller's stick or
 * shoulder cluster has a domain-specific name (e.g. motion-mapped axes).
 */
struct ControllerTypeDef {
    QString id;
    QString displayName;
    QString svgResource;
    QHash<QString, QString> slotTitleOverrides = {};
};
