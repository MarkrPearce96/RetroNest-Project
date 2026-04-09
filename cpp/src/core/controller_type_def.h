#pragma once

#include <QString>

/**
 * ControllerTypeDef — describes an available controller type for an emulator.
 */
struct ControllerTypeDef {
    QString id;          // "DualShock2", "Guitar", "NotConnected", etc.
    QString displayName; // "DualShock 2", "Guitar", etc.
    QString svgResource; // resource path: ":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg"
};
