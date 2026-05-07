#pragma once

#include <QString>

/**
 * BindingDef — describes a single controller button/axis binding.
 *
 * The schema-driven controller mapping view (controller_bindings_view)
 * uses `cardSlot` to place each binding's card in one of six fixed
 * page slots (DPad / FaceButtons / LeftAnalog / RightAnalog / Shoulders
 * / System) and uses `spotlightX/Y/R` (in the controller SVG's viewBox
 * coordinate system) to draw the OpenEmu-style spotlight at the
 * physical button when the card has focus. {0,0,0} means "no
 * spotlight" — used for abstract bindings (motors, pressure modifier)
 * that don't correspond to a specific button on the artwork.
 *
 * Adapters that haven't migrated to the new view ignore these fields;
 * defaulted values keep them harmless.
 */
struct BindingDef {
    enum Kind { Button, Axis };

    Kind kind;
    QString label;        // "Cross", "L1", "Left Stick Up"
    QString group;        // "Face Buttons", "Triggers", "Left Stick"
    QString section;      // INI section: "Pad1" / "Controller1"
    QString key;          // INI key: "Cross" / "ButtonCross"
    QString defaultValue; // "SDL-0/+A"

    // Card placement on the schema-driven page.
    // Allowed values: "DPad", "FaceButtons", "LeftAnalog", "RightAnalog",
    // "Shoulders", "System". Empty → fall back to matching `group`
    // case-insensitively (after stripping spaces).
    QString cardSlot = {};

    // Spotlight target on the controller SVG, in viewBox coordinates.
    // {0,0,0} = no spotlight (overlay suppressed when this binding has
    // focus).
    int spotlightX = 0;
    int spotlightY = 0;
    int spotlightR = 0;
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
