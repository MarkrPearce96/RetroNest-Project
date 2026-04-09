#pragma once

#include <QString>
#include <QMap>
#include "core/sdl_input_manager.h"

/**
 * Extract the SDL device index from a binding string like "SDL-0/DPadUp".
 * Returns -1 if the format doesn't match.
 */
inline int deviceIndexFromBinding(const QString& binding) {
    if (!binding.startsWith("SDL-")) return -1;
    int slashIdx = binding.indexOf('/');
    if (slashIdx < 0) return -1;
    bool ok;
    int idx = binding.mid(4, slashIdx - 4).toInt(&ok);
    return ok ? idx : -1;
}

/**
 * Convert a raw PCSX2 binding string (e.g. "SDL-0/DPadUp") to a
 * human-readable display string (e.g. "SDL-0 D-Pad Up") matching
 * PCSX2's native UI format, with controller-type-specific names.
 */
inline QString displayBinding(const QString& raw,
                               SdlInputManager::DetailedControllerType type =
                                   SdlInputManager::TypeStandard) {
    if (raw.isEmpty()) return {};

    // Keyboard bindings pass through as-is
    if (raw.startsWith("Keyboard/"))
        return raw;

    // Split on first '/' → prefix = "SDL-0", element = "DPadUp" or "+LeftY"
    int slashIdx = raw.indexOf('/');
    if (slashIdx < 0) return raw;

    QString prefix = raw.left(slashIdx);
    QString element = raw.mid(slashIdx + 1);

    // Strip leading +/- for lookup, preserve sign for axes
    QString sign;
    QString name = element;
    if (name.startsWith('+') || name.startsWith('-')) {
        sign = name.left(1);
        name = name.mid(1);
    }

    // --- Button display names per controller type (matching PCSX2 exactly) ---

    // Standard/Unknown (fallback)
    static const QMap<QString, QString> kStandardButtons = {
        {"FaceSouth", "Face South"}, {"FaceEast", "Face East"},
        {"FaceWest", "Face West"},   {"FaceNorth", "Face North"},
        {"Back", "Back"},            {"Guide", "Guide"},          {"Start", "Start"},
        {"LeftStick", "Left Stick"}, {"RightStick", "Right Stick"},
        {"LeftShoulder", "Left Shoulder"}, {"RightShoulder", "Right Shoulder"},
        {"DPadUp", "D-Pad Up"},     {"DPadDown", "D-Pad Down"},
        {"DPadLeft", "D-Pad Left"}, {"DPadRight", "D-Pad Right"},
        {"Misc1", "Misc 1"},        {"Touchpad", "Touchpad"},
    };

    // Xbox — same as standard but face buttons use A/B/X/Y labels
    static const QMap<QString, QString> kXboxButtons = {
        {"FaceSouth", "A"},          {"FaceEast", "B"},
        {"FaceWest", "X"},           {"FaceNorth", "Y"},
        {"Back", "Back"},            {"Guide", "Xbox"},           {"Start", "Start"},
        {"LeftStick", "Left Stick"}, {"RightStick", "Right Stick"},
        {"LeftShoulder", "Left Shoulder"}, {"RightShoulder", "Right Shoulder"},
        {"DPadUp", "D-Pad Up"},     {"DPadDown", "D-Pad Down"},
        {"DPadLeft", "D-Pad Left"}, {"DPadRight", "D-Pad Right"},
    };

    static const QMap<QString, QString> kPS3Buttons = {
        {"FaceSouth", "Cross"},      {"FaceEast", "Circle"},
        {"FaceWest", "Square"},      {"FaceNorth", "Triangle"},
        {"Back", "Select"},          {"Guide", "PS"},             {"Start", "Start"},
        {"LeftStick", "Left Stick"}, {"RightStick", "Right Stick"},
        {"LeftShoulder", "L1"},      {"RightShoulder", "R1"},
        {"DPadUp", "D-Pad Up"},     {"DPadDown", "D-Pad Down"},
        {"DPadLeft", "D-Pad Left"}, {"DPadRight", "D-Pad Right"},
    };

    static const QMap<QString, QString> kPS4Buttons = {
        {"FaceSouth", "Cross"},      {"FaceEast", "Circle"},
        {"FaceWest", "Square"},      {"FaceNorth", "Triangle"},
        {"Back", "Share"},           {"Guide", "PS"},             {"Start", "Options"},
        {"LeftStick", "Left Stick"}, {"RightStick", "Right Stick"},
        {"LeftShoulder", "L1"},      {"RightShoulder", "R1"},
        {"DPadUp", "D-Pad Up"},     {"DPadDown", "D-Pad Down"},
        {"DPadLeft", "D-Pad Left"}, {"DPadRight", "D-Pad Right"},
    };

    static const QMap<QString, QString> kPS5Buttons = {
        {"FaceSouth", "Cross"},      {"FaceEast", "Circle"},
        {"FaceWest", "Square"},      {"FaceNorth", "Triangle"},
        {"Back", "Create"},          {"Guide", "PS"},             {"Start", "Options"},
        {"LeftStick", "Left Stick"}, {"RightStick", "Right Stick"},
        {"LeftShoulder", "L1"},      {"RightShoulder", "R1"},
        {"DPadUp", "D-Pad Up"},     {"DPadDown", "D-Pad Down"},
        {"DPadLeft", "D-Pad Left"}, {"DPadRight", "D-Pad Right"},
        {"Misc1", "Mute"},          {"Touchpad", "Touchpad"},
    };

    // --- Axis display names (shared, but triggers vary by type) ---

    static const QMap<QString, QString> kAxisNames = {
        {"LeftX", "Left X"},   {"LeftY", "Left Y"},
        {"RightX", "Right X"}, {"RightY", "Right Y"},
    };

    static const QMap<QString, QString> kTriggerStandard = {
        {"LeftTrigger", "Left Trigger"}, {"RightTrigger", "Right Trigger"},
    };

    static const QMap<QString, QString> kTriggerPS = {
        {"LeftTrigger", "L2"}, {"RightTrigger", "R2"},
    };

    // --- Motor display names ---
    static const QMap<QString, QString> kMotors = {
        {"LargeMotor", "Large Motor"}, {"SmallMotor", "Small Motor"},
    };

    // --- Select the right button table ---
    const QMap<QString, QString>* buttonMap = &kStandardButtons;
    bool isPS = false;
    switch (type) {
    case SdlInputManager::TypeXbox360:
    case SdlInputManager::TypeXboxOne:
        buttonMap = &kXboxButtons;
        break;
    case SdlInputManager::TypePS3:
        buttonMap = &kPS3Buttons;
        isPS = true;
        break;
    case SdlInputManager::TypePS4:
        buttonMap = &kPS4Buttons;
        isPS = true;
        break;
    case SdlInputManager::TypePS5:
        buttonMap = &kPS5Buttons;
        isPS = true;
        break;
    default:
        break;
    }

    // --- Lookup in order: buttons, axes, triggers, motors ---
    QString display;

    auto btnIt = buttonMap->find(name);
    if (btnIt != buttonMap->end()) {
        display = btnIt.value();
    } else {
        auto axIt = kAxisNames.find(name);
        if (axIt != kAxisNames.end()) {
            display = axIt.value();
        } else {
            auto trIt = isPS ? kTriggerPS.find(name) : kTriggerStandard.find(name);
            auto trEnd = isPS ? kTriggerPS.end() : kTriggerStandard.end();
            if (trIt != trEnd) {
                display = trIt.value();
            } else {
                auto motIt = kMotors.find(name);
                if (motIt != kMotors.end())
                    display = motIt.value();
                else
                    display = name; // fallback: raw name
            }
        }
    }

    // For stick axes, keep the +/- sign prefix
    bool isStickAxis = kAxisNames.contains(name);
    if (isStickAxis && !sign.isEmpty())
        return prefix + " " + sign + display;

    return prefix + " " + display;
}
