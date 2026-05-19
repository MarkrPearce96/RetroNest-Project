#include <QtTest>
#include "core/libretro/hotkey_dispatcher.h"
#include "core/libretro/libretro_hotkey_defs.h"

class TestHotkeyDispatcher : public QObject {
    Q_OBJECT
private:
    // Counters accumulated by callbacks for inspection by tests.
    struct Counters {
        int saves = 0, loads = 0, lastSlot = -1;
        int ffToggles = 0;
        int ffHold = 0;          // +1 on true, -1 on false
        int pauseToggles = 0;
        int resets = 0;
        int menuOpens = 0;
        int muteToggles = 0;
        int volumeDeltaSum = 0;
        int slotSet = -1;
        int currentSlotProbe = 2;  // pretend slot 2 is current
    };

    HotkeyDispatcher::Callbacks makeCallbacks(Counters* c) {
        HotkeyDispatcher::Callbacks cb;
        cb.saveStateSlot   = [c](int s) { c->saves++; c->lastSlot = s; };
        cb.loadStateSlot   = [c](int s) { c->loads++; c->lastSlot = s; };
        cb.getCurrentSlot  = [c]() { return c->currentSlotProbe; };
        cb.setCurrentSlot  = [c](int s) { c->slotSet = s; };
        cb.toggleFastForward = [c]() { c->ffToggles++; };
        cb.setFastForward  = [c](bool on) { c->ffHold += on ? 1 : -1; };
        cb.togglePause     = [c]() { c->pauseToggles++; };
        cb.reset           = [c]() { c->resets++; };
        cb.openMenu        = [c]() { c->menuOpens++; };
        cb.toggleMute      = [c]() { c->muteToggles++; };
        cb.adjustVolume    = [c](int d) { c->volumeDeltaSum += d; };
        return cb;
    }

private slots:
    void saveStateUsesCurrentSlot() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::SaveState);
        QCOMPARE(c.saves, 1);
        QCOMPARE(c.lastSlot, 2);  // matches getCurrentSlot() probe
    }
    void loadStateUsesCurrentSlot() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::LoadState);
        QCOMPARE(c.loads, 1);
        QCOMPARE(c.lastSlot, 2);
    }
    void saveStateSlot3UsesExplicitSlot() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::SaveStateSlot(3));
        QCOMPARE(c.saves, 1);
        QCOMPARE(c.lastSlot, 3);
    }
    void loadStateSlot5UsesExplicitSlot() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::LoadStateSlot(5));
        QCOMPARE(c.loads, 1);
        QCOMPARE(c.lastSlot, 5);
    }
    void nextSlotIncrementsCurrent() {
        Counters c;
        c.currentSlotProbe = 2;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::NextSlot);
        QCOMPARE(c.slotSet, 3);
    }
    void prevSlotDecrementsCurrent() {
        Counters c;
        c.currentSlotProbe = 2;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::PrevSlot);
        QCOMPARE(c.slotSet, 1);
    }
    void fastForwardToggleCallsToggle() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::FastForwardToggle);
        QCOMPARE(c.ffToggles, 1);
    }
    void fastForwardHoldOnPressAndRelease() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::FastForwardHold);
        QCOMPARE(c.ffHold, 1);
        d.onActionReleased(libretro_hotkeys::ids::FastForwardHold);
        QCOMPARE(c.ffHold, 0);
    }
    void pauseTogglesAndResetCalls() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::Pause);
        QCOMPARE(c.pauseToggles, 1);
        d.onActionPressed(libretro_hotkeys::ids::Reset);
        QCOMPARE(c.resets, 1);
    }
    void menuAndAudioActions() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(libretro_hotkeys::ids::ToggleMenu);
        d.onActionPressed(libretro_hotkeys::ids::Mute);
        d.onActionPressed(libretro_hotkeys::ids::VolumeUp);
        d.onActionPressed(libretro_hotkeys::ids::VolumeDown);
        QCOMPARE(c.menuOpens, 1);
        QCOMPARE(c.muteToggles, 1);
        QCOMPARE(c.volumeDeltaSum, 0);  // +10 + -10
    }
    void unknownActionIgnored() {
        Counters c;
        HotkeyDispatcher d(makeCallbacks(&c));
        d.onActionPressed(QStringLiteral("Nonsense"));
        d.onActionReleased(QStringLiteral("Nonsense"));
        QCOMPARE(c.saves, 0);
        QCOMPARE(c.menuOpens, 0);
    }
    void nullCallbacksDoNotCrash() {
        HotkeyDispatcher::Callbacks empty;  // all nullptrs
        HotkeyDispatcher d(empty);
        d.onActionPressed(libretro_hotkeys::ids::SaveState);
        d.onActionPressed(libretro_hotkeys::ids::ToggleMenu);
        d.onActionReleased(libretro_hotkeys::ids::FastForwardHold);
        // No crash = pass.
    }
};

QTEST_APPLESS_MAIN(TestHotkeyDispatcher)
#include "test_hotkey_dispatcher.moc"
