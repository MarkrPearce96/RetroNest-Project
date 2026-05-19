#include "core/libretro/hotkey_dispatcher.h"
#include "core/libretro/libretro_hotkey_defs.h"

namespace ids = libretro_hotkeys::ids;

HotkeyDispatcher::HotkeyDispatcher(Callbacks cb, QObject* parent)
    : QObject(parent), m_cb(std::move(cb)) {}

void HotkeyDispatcher::onActionPressed(const QString& a) {
    if (a == ids::ToggleMenu) { if (m_cb.openMenu) m_cb.openMenu(); return; }
    if (a == ids::FastForwardToggle) { if (m_cb.toggleFastForward) m_cb.toggleFastForward(); return; }
    if (a == ids::FastForwardHold)   { if (m_cb.setFastForward) m_cb.setFastForward(true); return; }
    if (a == ids::Pause)  { if (m_cb.togglePause) m_cb.togglePause(); return; }
    if (a == ids::Reset)  { if (m_cb.reset) m_cb.reset(); return; }
    if (a == ids::SaveState) {
        if (m_cb.saveStateSlot && m_cb.getCurrentSlot) m_cb.saveStateSlot(m_cb.getCurrentSlot());
        return;
    }
    if (a == ids::LoadState) {
        if (m_cb.loadStateSlot && m_cb.getCurrentSlot) m_cb.loadStateSlot(m_cb.getCurrentSlot());
        return;
    }
    if (a == ids::NextSlot) {
        if (m_cb.getCurrentSlot && m_cb.setCurrentSlot) m_cb.setCurrentSlot(m_cb.getCurrentSlot() + 1);
        return;
    }
    if (a == ids::PrevSlot) {
        if (m_cb.getCurrentSlot && m_cb.setCurrentSlot) m_cb.setCurrentSlot(m_cb.getCurrentSlot() - 1);
        return;
    }
    for (int n = 1; n <= 5; ++n) {
        if (a == ids::SaveStateSlot(n)) { if (m_cb.saveStateSlot) m_cb.saveStateSlot(n); return; }
        if (a == ids::LoadStateSlot(n)) { if (m_cb.loadStateSlot) m_cb.loadStateSlot(n); return; }
    }
    if (a == ids::Mute)       { if (m_cb.toggleMute) m_cb.toggleMute(); return; }
    if (a == ids::VolumeUp)   { if (m_cb.adjustVolume) m_cb.adjustVolume(+10); return; }
    if (a == ids::VolumeDown) { if (m_cb.adjustVolume) m_cb.adjustVolume(-10); return; }
    // Unknown action — no-op.
}

void HotkeyDispatcher::onActionReleased(const QString& a) {
    if (a == ids::FastForwardHold && m_cb.setFastForward) m_cb.setFastForward(false);
}
