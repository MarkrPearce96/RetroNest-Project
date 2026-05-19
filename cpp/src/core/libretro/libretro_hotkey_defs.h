#pragma once

#include <QString>
#include <QVector>
#include "core/binding_def.h"

namespace libretro_hotkeys {

inline const QString kSentinelEmuId = QStringLiteral("_libretro_global");

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
