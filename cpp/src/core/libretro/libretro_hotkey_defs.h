#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include "core/binding_def.h"

namespace libretro_hotkeys {

inline const QString kSentinelEmuId = QStringLiteral("_libretro_global");

// Split a " & "-joined binding string into keyboard-only and gamepad-only
// halves. Either half may be empty. Used by the dual-column hotkey UI to
// show keyboard / controller bindings independently while keeping a single
// combined string in storage.
inline void splitBindingByType(const QString& combined,
                                QString* outKeyboard,
                                QString* outGamepad) {
    QStringList kbd, pad;
    const QStringList tokens = combined.split(QStringLiteral(" & "),
                                              Qt::SkipEmptyParts);
    for (const QString& tokenRaw : tokens) {
        const QString token = tokenRaw.trimmed();
        if (token.isEmpty()) continue;
        if (token.startsWith(QStringLiteral("Keyboard/"))) kbd << token;
        else if (token.startsWith(QStringLiteral("Gamepad"))) pad << token;
        // Other prefixes (SDL-x/y, raw button names from adapters) are dropped.
    }
    if (outKeyboard) *outKeyboard = kbd.join(QStringLiteral(" & "));
    if (outGamepad)  *outGamepad  = pad.join(QStringLiteral(" & "));
}

// Merge separate keyboard + gamepad binding halves back into the
// combined storage format. Either side may be empty.
inline QString mergeBindingsByType(const QString& kbd, const QString& pad) {
    if (kbd.isEmpty()) return pad;
    if (pad.isEmpty()) return kbd;
    return kbd + QStringLiteral(" & ") + pad;
}

namespace ids {
    inline const QString ToggleMenu        = QStringLiteral("ToggleMenu");
    inline const QString FastForwardToggle = QStringLiteral("FastForwardToggle");
    inline const QString FastForwardHold   = QStringLiteral("FastForwardHold");
    inline const QString Pause             = QStringLiteral("Pause");
    inline const QString Reset             = QStringLiteral("Reset");
    inline const QString SaveState         = QStringLiteral("SaveState");
    inline const QString LoadState         = QStringLiteral("LoadState");
    inline const QString NextSlot          = QStringLiteral("NextSlot");
    inline const QString PrevSlot          = QStringLiteral("PrevSlot");
    inline const QString Mute              = QStringLiteral("Mute");
    inline const QString VolumeUp          = QStringLiteral("VolumeUp");
    inline const QString VolumeDown        = QStringLiteral("VolumeDown");

    inline QString SaveStateSlot(int n) {
        return QStringLiteral("SaveStateSlot%1").arg(n);
    }

    inline QString LoadStateSlot(int n) {
        return QStringLiteral("LoadStateSlot%1").arg(n);
    }
}

extern const QVector<HotkeyDef> kLibretroHotkeys;

} // namespace libretro_hotkeys
