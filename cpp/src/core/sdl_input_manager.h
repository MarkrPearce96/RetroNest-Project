#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QMap>
#include <QHash>
#include <QWindow>
#include <QGuiApplication>
#include <QKeyEvent>
#include <SDL2/SDL.h>

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
     * Switch to emulation mode: SDL gamepad button events write into the
     * InputRouter instead of being injected as Qt key events. The in-game
     * menu hotkey (Select+Circle) still fires inGameMenuRequested().
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
    void inGameMenuRequested(); // Select+B/Circle combo — in-game menu


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

    // Emulation mode: non-null while a libretro game is running
    InputRouter* m_emulationTarget = nullptr;
    // Hardcoded SDL button → RetroPad slot mapping (set once in setEmulationMode)
    QHash<SDL_GameControllerButton, RetroPadSlot> m_emulationMap;
};
