#include "core/libretro/libretro_hotkey_defs.h"

namespace libretro_hotkeys {

namespace {
    static HotkeyDef make(const QString& label, const QString& group,
                          const QString& key, const QString& defaultValue) {
        return HotkeyDef{label, group, QStringLiteral("Hotkeys"), key, defaultValue};
    }
}

// 22-row action table.
// 9 base + 5 SaveStateSlot + 5 LoadStateSlot + 3 audio = 22 total.
const QVector<HotkeyDef> kLibretroHotkeys = {
    // Navigation
    make(QStringLiteral("Toggle In-Game Menu"), QStringLiteral("Navigation"),
         ids::ToggleMenu, QStringLiteral("Keyboard/Escape")),

    // Speed
    make(QStringLiteral("Fast Forward (Toggle)"), QStringLiteral("Speed"),
         ids::FastForwardToggle, QStringLiteral("Keyboard/Space")),
    make(QStringLiteral("Fast Forward (Hold)"), QStringLiteral("Speed"),
         ids::FastForwardHold, QStringLiteral("")),

    // System
    make(QStringLiteral("Pause / Resume"), QStringLiteral("System"),
         ids::Pause, QStringLiteral("Keyboard/P")),
    make(QStringLiteral("Reset"), QStringLiteral("System"),
         ids::Reset, QStringLiteral("Keyboard/H")),

    // Save States (base 4) — F-keys avoided so the defaults work on macOS
    // without flipping the "Use F1, F2, etc. as standard function keys"
    // system setting.
    make(QStringLiteral("Save State (Current Slot)"), QStringLiteral("Save States"),
         ids::SaveState, QStringLiteral("Keyboard/,")),
    make(QStringLiteral("Load State (Current Slot)"), QStringLiteral("Save States"),
         ids::LoadState, QStringLiteral("Keyboard/.")),
    make(QStringLiteral("Next Save Slot"), QStringLiteral("Save States"),
         ids::NextSlot, QStringLiteral("Keyboard/]")),
    make(QStringLiteral("Previous Save Slot"), QStringLiteral("Save States"),
         ids::PrevSlot, QStringLiteral("Keyboard/[")),

    // SaveStateSlot1..5 — number row, Shift adds explicit slot.
    make(QStringLiteral("Save State to Slot 1"), QStringLiteral("Save States"),
         ids::SaveStateSlot(1), QStringLiteral("Keyboard/Shift+1")),
    make(QStringLiteral("Save State to Slot 2"), QStringLiteral("Save States"),
         ids::SaveStateSlot(2), QStringLiteral("Keyboard/Shift+2")),
    make(QStringLiteral("Save State to Slot 3"), QStringLiteral("Save States"),
         ids::SaveStateSlot(3), QStringLiteral("Keyboard/Shift+3")),
    make(QStringLiteral("Save State to Slot 4"), QStringLiteral("Save States"),
         ids::SaveStateSlot(4), QStringLiteral("Keyboard/Shift+4")),
    make(QStringLiteral("Save State to Slot 5"), QStringLiteral("Save States"),
         ids::SaveStateSlot(5), QStringLiteral("Keyboard/Shift+5")),

    // LoadStateSlot1..5
    make(QStringLiteral("Load State from Slot 1"), QStringLiteral("Save States"),
         ids::LoadStateSlot(1), QStringLiteral("Keyboard/Ctrl+1")),
    make(QStringLiteral("Load State from Slot 2"), QStringLiteral("Save States"),
         ids::LoadStateSlot(2), QStringLiteral("Keyboard/Ctrl+2")),
    make(QStringLiteral("Load State from Slot 3"), QStringLiteral("Save States"),
         ids::LoadStateSlot(3), QStringLiteral("Keyboard/Ctrl+3")),
    make(QStringLiteral("Load State from Slot 4"), QStringLiteral("Save States"),
         ids::LoadStateSlot(4), QStringLiteral("Keyboard/Ctrl+4")),
    make(QStringLiteral("Load State from Slot 5"), QStringLiteral("Save States"),
         ids::LoadStateSlot(5), QStringLiteral("Keyboard/Ctrl+5")),

    // Audio
    make(QStringLiteral("Toggle Mute"), QStringLiteral("Audio"),
         ids::Mute, QStringLiteral("Keyboard/M")),
    make(QStringLiteral("Volume Up"), QStringLiteral("Audio"),
         ids::VolumeUp, QStringLiteral("Keyboard/+")),
    make(QStringLiteral("Volume Down"), QStringLiteral("Audio"),
         ids::VolumeDown, QStringLiteral("Keyboard/-")),
};

} // namespace libretro_hotkeys
