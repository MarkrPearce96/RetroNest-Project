#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QMap>
#include <QWindow>
#include <QGuiApplication>
#include <QKeyEvent>
#include <SDL2/SDL.h>
#include <atomic>
#include <mutex>

#include "core/libretro/input_router.h"

/**
 * SdlInputManager — polls SDL2 for gamepad/keyboard events.
 * Injects Qt key events for navigation; provides "capture mode" for press-to-bind UI.
 */
class SdlInputManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool capturing READ isCapturing NOTIFY capturingChanged)
    Q_PROPERTY(QVariantList connectedControllers READ connectedControllers NOTIFY controllersChanged)
    Q_PROPERTY(bool lastInputWasController READ lastInputWasController NOTIFY lastInputWasControllerChanged)
    Q_PROPERTY(bool virtualKeyboardOpen READ virtualKeyboardOpen WRITE setVirtualKeyboardOpen NOTIFY virtualKeyboardOpenChanged)
    Q_PROPERTY(int controllerType READ controllerType NOTIFY controllerTypeChanged)

public:
    explicit SdlInputManager(QObject* parent = nullptr);
    ~SdlInputManager();

    enum ControllerType { Keyboard, Xbox, PlayStation };
    Q_ENUM(ControllerType)

    enum DetailedControllerType {
        TypeStandard = 0,
        TypeXbox360,
        TypeXboxOne,
        TypePS3,
        TypePS4,
        TypePS5,
        TypeSwitch
    };

    DetailedControllerType detailedControllerTypeForDevice(int deviceIndex) const;

    bool isCapturing() const;
    QVariantList connectedControllers() const;
    /** Device indices (0-based, lowest-available scheme) of all currently
     *  connected controllers. Used to compute per-port presence for the
     *  libretro controller-port-device notification. */
    QList<int> connectedDeviceIndices() const;
    bool lastInputWasController() const;
    bool virtualKeyboardOpen() const;
    void setVirtualKeyboardOpen(bool open);
    int controllerType() const;
    ControllerType controllerTypeForDevice(int deviceIndex) const;

    void setWindow(QWindow* window);
    bool eventFilter(QObject* obj, QEvent* event) override;

    Q_INVOKABLE void startCapture();
    Q_INVOKABLE void cancelCapture();
    void setCaptureMode(bool multi) { m_multiCapture = multi; }

    /** Convert SDL button/axis name to canonical name (e.g. "a" -> "A", "dpup" -> "DPadUp") */
    static QString canonicalName(const char* sdlName);

    /**
     * True if any A/B/X/Y face button is currently held down on any
     * connected controller. Used by AppController to defer SIGCONT
     * until the close-trigger button is released — the emulator polls
     * SDL on its first frame after resume, so any held button would
     * leak as in-game input.
     */
    bool isAnyActionButtonPressed() const;

    static constexpr uint32_t kRumbleDurationMs = 100;

    /**
     * Fire one motor on the controller mapped to `port`. PCSX2's rumble
     * interface delivers STRONG and WEAK as separate per-motor calls; we
     * cache the last value of each and merge before invoking SDL so both
     * motors stay alive when the core only updates one.
     *
     * Returns false if no controller is currently mapped to `port` (e.g.
     * disconnected mid-game) — the libretro contract has no failure path
     * for set_rumble_state, so the caller can ignore the return.
     */
    bool setRumbleMotor(int port, unsigned motor, uint16_t strength);

    /**
     * Switch to emulation mode: SDL gamepad button events write into the
     * InputRouter instead of being injected as Qt key events. The in-game
     * menu hotkey (Select+Start, or Touchpad on PS controllers)
     * still fires inGameMenuRequested().
     * Call clearEmulationMode() when the libretro game ends.
     */
    void setEmulationMode(InputRouter* target);
    void clearEmulationMode();

signals:
    void capturingChanged();
    void controllersChanged();
    void lastInputWasControllerChanged();
    void virtualKeyboardOpenChanged();
    void bindingCaptured(int deviceIndex, const QString& element, bool isAxis, bool positive);
    void keyboardCaptured(const QString& keyString);
    void captureButtonReleased(); // emitted when any captured button is released (multi-capture mode)
    void controllerTypeChanged();

    // Non-navigation signals (app-level actions, not injected as keys)
    void navigateStart();       // Start button — toggle settings
    void navigateShift();       // R2 trigger — shift/caps toggle
    void inGameMenuRequested(); // Select+Start combo or Touchpad — in-game menu

    /**
     * Fired on every controller button edge while emulation mode is active.
     * `port` is the libretro device index (matches InputRouter ports), and
     * `button` is the libretro RETRO_DEVICE_ID_JOYPAD_* index (matches
     * static_cast<int>(RetroPadSlot)). Wired in AppController to feed
     * HotkeyMatcher::onGamepadButton for the libretro global hotkey path.
     */
    void gamepadButtonChanged(int port, int button, bool pressed);


private:
    void pollEvents();
    void injectKey(int qtKey, QEvent::Type type);
    void openController(int joystickIndex);
    void closeController(SDL_JoystickID instanceId);

    QTimer* m_pollTimer = nullptr;
    QWindow* m_window = nullptr;
    bool m_capturing = false;
    bool m_multiCapture = false;
    bool m_sdlInitialized = false;
    bool m_lastInputWasController = false;
    bool m_virtualKeyboardOpen = false;
    bool m_injectingKey = false;  // true while injecting a controller key event
    bool m_selectHeld = false;    // true while Select/Back button is held (combo detection)

    // Axis threshold-crossing state (shared between above/below threshold checks)
    QMap<QPair<SDL_JoystickID, int>, bool> m_axisActive;

    // instance ID -> controller
    QMap<SDL_JoystickID, SDL_GameController*> m_controllers;
    // instance ID -> device index (0-based)
    QMap<SDL_JoystickID, int> m_deviceIndices;
    ControllerType m_activeControllerType = Xbox;
    QMap<SDL_JoystickID, ControllerType> m_controllerTypes;

    struct RumbleCache {
        std::atomic<uint16_t> low{0};   // RETRO_RUMBLE_STRONG
        std::atomic<uint16_t> high{0};  // RETRO_RUMBLE_WEAK
    };
    RumbleCache m_rumbleCache[InputRouter::NUM_PORTS];

    // Guards m_controllers / m_deviceIndices access and the SDL_GameController*
    // pointer lifetime against the core thread's setRumbleMotor reads. Without
    // this mutex, openController/closeController on the Qt thread can mutate
    // QMaps and SDL_GameControllerClose handles while the core thread is
    // mid-iterate / mid-SDL_GameControllerRumble — both are use-after-free
    // races. Acquired by setRumbleMotor (core thread) and by
    // openController/closeController (Qt thread).
    mutable std::mutex m_controllerMx;

    // Emulation mode: non-null while a libretro game is running.
    // Button routing uses InputRouter::lookup() — no separate hardcoded map.
    InputRouter* m_emulationTarget = nullptr;
};
