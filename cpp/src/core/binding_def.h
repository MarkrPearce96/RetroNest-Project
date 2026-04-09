#pragma once

#include <QString>

/**
 * BindingDef — describes a single controller button/axis binding.
 */
struct BindingDef {
    enum Kind { Button, Axis };

    Kind kind;
    QString label;        // "Cross", "L1", "Left Stick Up"
    QString group;        // "Face Buttons", "Triggers", "Left Stick"
    QString section;      // INI section: "Pad1" / "Controller1"
    QString key;          // INI key: "Cross" / "ButtonCross"
    QString defaultValue; // "SDL-0/+A"
};

/**
 * HotkeyDef — describes a single hotkey binding.
 */
struct HotkeyDef {
    QString label;        // "Toggle Pause"
    QString group;        // "General", "Save States"
    QString section;      // "Hotkeys"
    QString key;          // "TogglePause"
    QString defaultValue; // "Keyboard/Space"
};
