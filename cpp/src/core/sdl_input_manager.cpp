#include "sdl_input_manager.h"
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSet>
#include <QVariantMap>
#include <cmath>

// Deadzone threshold for axis capture (out of 32767)
static constexpr int kAxisDeadzone = 16000;

// SDL button/axis name -> canonical name mapping
static const QMap<QString, QString>& canonicalMap() {
    static const QMap<QString, QString> map = {
        // Buttons
        {"a", "FaceSouth"}, {"b", "FaceEast"}, {"x", "FaceWest"}, {"y", "FaceNorth"},
        {"back", "Back"}, {"guide", "Guide"}, {"start", "Start"},
        {"leftstick", "LeftStick"}, {"rightstick", "RightStick"},
        {"leftshoulder", "LeftShoulder"}, {"rightshoulder", "RightShoulder"},
        {"dpup", "DPadUp"}, {"dpdown", "DPadDown"},
        {"dpleft", "DPadLeft"}, {"dpright", "DPadRight"},
        // Axes
        {"leftx", "LeftX"}, {"lefty", "LeftY"},
        {"rightx", "RightX"}, {"righty", "RightY"},
        {"lefttrigger", "LeftTrigger"}, {"righttrigger", "RightTrigger"},
    };
    return map;
}

// SDL keycode -> key name for INI format
static QString keyName(SDL_Keycode key) {
    switch (key) {
    case SDLK_RETURN:    return "Return";
    case SDLK_ESCAPE:    return "Escape";
    case SDLK_BACKSPACE: return "Backspace";
    case SDLK_TAB:       return "Tab";
    case SDLK_SPACE:     return "Space";
    case SDLK_DELETE:    return "Delete";
    case SDLK_LSHIFT: case SDLK_RSHIFT: return "Shift";
    case SDLK_LCTRL:  case SDLK_RCTRL:  return "Control";
    case SDLK_LALT:   case SDLK_RALT:   return "Alt";
    case SDLK_PERIOD:    return "Period";
    case SDLK_COMMA:     return "Comma";
    case SDLK_SLASH:     return "Slash";
    default: break;
    }
    if (key >= SDLK_F1 && key <= SDLK_F12)
        return QString("F%1").arg(key - SDLK_F1 + 1);
    const char* name = SDL_GetKeyName(key);
    if (name && name[0] != '\0')
        return QString(name);
    return QString("Key%1").arg(key);
}

// Qt::Key code -> key name in the same canonical format that SDL's
// keyName() emits. PCSX2 (and other adapters) expect "Comma", "Period",
// "Slash", "Escape", "Delete" — not Qt's QKeySequence::toString output
// of ",", ".", "/", "Esc", "Del".
static QString qtKeyToCanonicalName(int qtKey) {
    switch (qtKey) {
    // Punctuation that QKeySequence renders as the literal character.
    case Qt::Key_Comma:        return "Comma";
    case Qt::Key_Period:       return "Period";
    case Qt::Key_Slash:        return "Slash";
    case Qt::Key_Backslash:    return "Backslash";
    case Qt::Key_Semicolon:    return "Semicolon";
    case Qt::Key_Apostrophe:   return "Apostrophe";
    case Qt::Key_QuoteLeft:    return "Grave";       // backtick
    case Qt::Key_BracketLeft:  return "LeftBracket";
    case Qt::Key_BracketRight: return "RightBracket";
    case Qt::Key_Minus:        return "Minus";
    case Qt::Key_Equal:        return "Equals";
    case Qt::Key_Plus:         return "Plus";

    // Names where QKeySequence abbreviates differently than SDL.
    case Qt::Key_Escape:       return "Escape";
    case Qt::Key_Delete:       return "Delete";
    case Qt::Key_Backspace:    return "Backspace";
    case Qt::Key_Return:
    case Qt::Key_Enter:        return "Return";
    case Qt::Key_Tab:          return "Tab";
    case Qt::Key_Backtab:      return "Tab";
    case Qt::Key_Space:        return "Space";

    // Modifier keys (return canonical single-name even if user pressed L/R variant).
    case Qt::Key_Shift:        return "Shift";
    case Qt::Key_Control:      return "Control";
    case Qt::Key_Alt:          return "Alt";
    case Qt::Key_Meta:         return "Meta";

    // Arrow / nav keys (Qt and SDL agree; here for clarity).
    case Qt::Key_Up:           return "Up";
    case Qt::Key_Down:         return "Down";
    case Qt::Key_Left:         return "Left";
    case Qt::Key_Right:        return "Right";
    case Qt::Key_Home:         return "Home";
    case Qt::Key_End:          return "End";
    case Qt::Key_PageUp:       return "PageUp";
    case Qt::Key_PageDown:     return "PageDown";
    case Qt::Key_Insert:       return "Insert";

    default: break;
    }

    // Function keys: F1..F35 in Qt's enum.
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F35)
        return QString("F%1").arg(qtKey - Qt::Key_F1 + 1);

    // Letters / digits — QKeySequence renders these the same as SDL.
    return QKeySequence(qtKey).toString();
}

// Controller button -> Qt key mapping
static int mapButtonToKey(SDL_GameControllerButton btn) {
    switch (btn) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    return Qt::Key_Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return Qt::Key_Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return Qt::Key_Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Qt::Key_Right;
    case SDL_CONTROLLER_BUTTON_A:          return Qt::Key_Return;
    case SDL_CONTROLLER_BUTTON_B:          return Qt::Key_Back;
    case SDL_CONTROLLER_BUTTON_X:          return Qt::Key_Backspace;
    case SDL_CONTROLLER_BUTTON_Y:          return Qt::Key_M;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return Qt::Key_Backtab;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Qt::Key_Tab;
    // Start handled as signal, not key injection (conflicts with Shortcuts)
    default: return 0;
    }
}

static SdlInputManager::ControllerType detectControllerType(const char* name) {
    if (!name) return SdlInputManager::Xbox;
    QString lower = QString(name).toLower();
    if (lower.contains("ps3") || lower.contains("ps4") || lower.contains("ps5") ||
        lower.contains("dualshock") || lower.contains("dualsense") ||
        lower.contains("playstation")) {
        return SdlInputManager::PlayStation;
    }
    return SdlInputManager::Xbox;
}

SdlInputManager::ControllerType SdlInputManager::controllerTypeForDevice(int deviceIndex) const {
    for (auto it = m_deviceIndices.constBegin(); it != m_deviceIndices.constEnd(); ++it) {
        if (it.value() == deviceIndex)
            return m_controllerTypes.value(it.key(), Xbox);
    }
    return Xbox;
}

SdlInputManager::DetailedControllerType SdlInputManager::detailedControllerTypeForDevice(int deviceIndex) const {
    for (auto it = m_deviceIndices.constBegin(); it != m_deviceIndices.constEnd(); ++it) {
        if (it.value() == deviceIndex) {
            auto* ctrl = m_controllers.value(it.key(), nullptr);
            if (ctrl) {
                SDL_GameControllerType sdlType = SDL_GameControllerGetType(ctrl);
                switch (sdlType) {
                case SDL_CONTROLLER_TYPE_XBOX360:          return TypeXbox360;
                case SDL_CONTROLLER_TYPE_XBOXONE:          return TypeXboxOne;
                case SDL_CONTROLLER_TYPE_PS3:               return TypePS3;
                case SDL_CONTROLLER_TYPE_PS4:               return TypePS4;
                case SDL_CONTROLLER_TYPE_PS5:               return TypePS5;
                case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return TypeSwitch;
                default:                                    return TypeStandard;
                }
            }
        }
    }
    return TypeStandard;
}

SdlInputManager::SdlInputManager(QObject* parent)
    : QObject(parent)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        qWarning() << "[SDL] Failed to init GameController:" << SDL_GetError();
        return;
    }
    m_sdlInitialized = true;
    qInfo() << "[SDL] Initialized GameController subsystem";

    int numJoy = SDL_NumJoysticks();
    qInfo() << "[SDL] Found" << numJoy << "joystick(s)";
    for (int i = 0; i < numJoy; ++i) {
        const char* name = SDL_JoystickNameForIndex(i);
        bool isCtrl = SDL_IsGameController(i);
        qInfo() << "[SDL] Joystick" << i << ":" << (name ? name : "unknown")
                << (isCtrl ? "(game controller)" : "(not a game controller)");
        if (isCtrl)
            openController(i);
    }

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &SdlInputManager::pollEvents);
    m_pollTimer->start(16); // ~60 Hz
}

SdlInputManager::~SdlInputManager() {
    if (m_pollTimer) m_pollTimer->stop();
    for (auto* ctrl : m_controllers)
        SDL_GameControllerClose(ctrl);
    if (m_sdlInitialized)
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

bool SdlInputManager::isCapturing() const { return m_capturing; }

bool SdlInputManager::lastInputWasController() const { return m_lastInputWasController; }

bool SdlInputManager::virtualKeyboardOpen() const { return m_virtualKeyboardOpen; }
void SdlInputManager::setVirtualKeyboardOpen(bool open) {
    if (m_virtualKeyboardOpen != open) {
        m_virtualKeyboardOpen = open;
        emit virtualKeyboardOpenChanged();
    }
}

int SdlInputManager::controllerType() const {
    if (!m_lastInputWasController) return Keyboard;
    return static_cast<int>(m_activeControllerType);
}

void SdlInputManager::setWindow(QWindow* window) {
    m_window = window;
    // Install event filter on qApp so keyboard-mode detection works
    // across all windows (main window, settings dialogs, etc.).
    QGuiApplication::instance()->installEventFilter(this);
}

bool SdlInputManager::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress && !m_injectingKey) {
        auto* ke = static_cast<QKeyEvent*>(event);
        const int key = ke->key();

        // Capture mode: turn the next non-modifier keypress into a
        // "Keyboard/<name>" binding string. Mirrors the SDL_KEYDOWN
        // path in pollEvents() — SDL only sees the keypress when an
        // SDL window has focus, which our QDialog doesn't have on
        // macOS, so this Qt-side path is the only reliable one for
        // keyboard captures from inside Qt dialogs.
        if (m_capturing) {
            // Skip pure-modifier keys (user is still building a chord).
            if (key == Qt::Key_Shift || key == Qt::Key_Control ||
                key == Qt::Key_Alt   || key == Qt::Key_Meta) {
                return false;
            }
            // Escape cancels capture instead of binding.
            if (key == Qt::Key_Escape) {
                cancelCapture();
                return true;
            }
            // Build a "Keyboard/<name>" string matching the format
            // emitted by the SDL_KEYDOWN path.
            const QString keyStr = qtKeyToCanonicalName(key);
            const auto mods = ke->modifiers();
            QString full;
            if      (mods & Qt::AltModifier)     full = "Keyboard/Alt & Keyboard/"     + keyStr;
            else if (mods & Qt::ShiftModifier)   full = "Keyboard/Shift & Keyboard/"   + keyStr;
            else if (mods & Qt::ControlModifier) full = "Keyboard/Control & Keyboard/" + keyStr;
            else                                 full = "Keyboard/" + keyStr;

            m_capturing = false;
            emit capturingChanged();
            emit keyboardCaptured(full);
            return true;   // consume — don't propagate to widgets / shortcuts.
        }

        // Outside capture: detect a real keyboard press to switch back
        // from controller-mode (preserves the existing behaviour).
        if (m_lastInputWasController) {
            m_lastInputWasController = false;
            emit lastInputWasControllerChanged();
            emit controllerTypeChanged();
        }
    }
    return QObject::eventFilter(obj, event);
}

QVariantList SdlInputManager::connectedControllers() const {
    QVariantList list;
    for (auto it = m_controllers.constBegin(); it != m_controllers.constEnd(); ++it) {
        QVariantMap entry;
        entry["instanceId"] = it.key();
        entry["deviceIndex"] = m_deviceIndices.value(it.key(), 0);
        entry["name"] = QString(SDL_GameControllerName(it.value()));
        list.append(entry);
    }
    return list;
}

void SdlInputManager::startCapture() {
    m_capturing = true;
    emit capturingChanged();
}

void SdlInputManager::cancelCapture() {
    m_capturing = false;
    m_multiCapture = false;
    emit capturingChanged();
}

bool SdlInputManager::isAnyActionButtonPressed() const {
    for (auto it = m_controllers.cbegin(); it != m_controllers.cend(); ++it) {
        SDL_GameController* ctrl = it.value();
        if (!ctrl) continue;
        if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A)) return true;
        if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_B)) return true;
        if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_X)) return true;
        if (SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_Y)) return true;
    }
    return false;
}

QString SdlInputManager::canonicalName(const char* sdlName) {
    if (!sdlName) return {};
    return canonicalMap().value(QString(sdlName).toLower(), QString(sdlName));
}

void SdlInputManager::setEmulationMode(InputRouter* target) {
    m_emulationTarget = target;
    qInfo() << "[SdlInput] Emulation mode ON — routing via InputRouter";
}

void SdlInputManager::clearEmulationMode() {
    m_emulationTarget = nullptr;
    qInfo() << "[SdlInput] Emulation mode OFF";
}

void SdlInputManager::injectKey(int qtKey, QEvent::Type type) {
    if (!m_window) return;
    m_injectingKey = true;
    if (!m_lastInputWasController) {
        m_lastInputWasController = true;
        emit lastInputWasControllerChanged();
    }
    QKeyEvent event(type, qtKey, Qt::NoModifier);
    // Send to the focused window (e.g. a QDialog) if one exists,
    // otherwise fall back to the main window.
    QWindow* target = QGuiApplication::focusWindow();
    if (!target) target = m_window;
    QGuiApplication::sendEvent(target, &event);
    m_injectingKey = false;
}

void SdlInputManager::openController(int joystickIndex) {
    SDL_GameController* ctrl = SDL_GameControllerOpen(joystickIndex);
    if (!ctrl) {
        qWarning() << "[SDL] Failed to open controller" << joystickIndex << ":" << SDL_GetError();
        return;
    }
    SDL_Joystick* joy = SDL_GameControllerGetJoystick(ctrl);
    SDL_JoystickID id = SDL_JoystickInstanceID(joy);

    // Skip if already tracked (SDL fires ADDED events for already-opened controllers)
    if (m_controllers.contains(id)) {
        SDL_GameControllerClose(ctrl);
        return;
    }
    m_controllers.insert(id, ctrl);

    // Assign lowest available device index (matches PCSX2's player_id scheme)
    int devIdx = 0;
    QSet<int> usedIndices;
    for (auto it = m_deviceIndices.constBegin(); it != m_deviceIndices.constEnd(); ++it)
        usedIndices.insert(it.value());
    while (usedIndices.contains(devIdx)) devIdx++;
    m_deviceIndices.insert(id, devIdx);

    m_controllerTypes.insert(id, detectControllerType(SDL_GameControllerName(ctrl)));
    qInfo() << "[SDL] Opened controller:" << SDL_GameControllerName(ctrl)
            << "as device" << devIdx;
    emit controllersChanged();
}

void SdlInputManager::closeController(SDL_JoystickID instanceId) {
    if (auto* ctrl = m_controllers.value(instanceId, nullptr)) {
        qInfo() << "[SDL] Controller disconnected:" << SDL_GameControllerName(ctrl);
        SDL_GameControllerClose(ctrl);
        m_controllers.remove(instanceId);
        m_deviceIndices.remove(instanceId);
        m_controllerTypes.remove(instanceId);
        emit controllersChanged();
    }
}

void SdlInputManager::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED:
            openController(event.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            closeController(event.cdevice.which);
            break;

        case SDL_CONTROLLERBUTTONDOWN:
            if (m_capturing) {
                int devIdx = m_deviceIndices.value(event.cbutton.which, 0);
                const char* btnName = SDL_GameControllerGetStringForButton(
                    static_cast<SDL_GameControllerButton>(event.cbutton.button));
                QString element = canonicalName(btnName);
                if (!m_multiCapture) {
                    m_capturing = false;
                    emit capturingChanged();
                }
                emit bindingCaptured(devIdx, element, false, true);
            } else if (m_emulationTarget) {
                // Emulation mode (libretro): route to InputRouter,
                // except for the in-game menu trigger. Select+B and
                // Touchpad both open the menu. The "pre-emptive pause"
                // signals are NOT emitted here — libretro's pause is
                // in-process (no SIGSTOP), and Select itself is a
                // valid in-game button for the libretro core.
                auto btn = static_cast<SDL_GameControllerButton>(event.cbutton.button);
                if (btn == SDL_CONTROLLER_BUTTON_BACK)
                    m_selectHeld = true;
                if (m_selectHeld && btn == SDL_CONTROLLER_BUTTON_START) {
                    qDebug() << "[SdlInput] Select+Start combo — emitting inGameMenuRequested (libretro)";
                    emit inGameMenuRequested();
                    break;
                }
                if (btn == SDL_CONTROLLER_BUTTON_TOUCHPAD) {
                    qDebug() << "[SdlInput] Touchpad press — emitting inGameMenuRequested (libretro)";
                    emit inGameMenuRequested();
                    break;
                }
                {
                    const int devIdx = m_deviceIndices.value(event.cbutton.which, 0);
                    const char* btnName = SDL_GameControllerGetStringForButton(btn);
                    const QString canonical = canonicalName(btnName);
                    const RetroPadSlot slot = m_emulationTarget->lookup(devIdx, canonical);
                    if (slot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, slot, true);
                }
            } else {
                auto type = m_controllerTypes.value(event.cbutton.which, Xbox);
                bool typeChanged = (type != m_activeControllerType || !m_lastInputWasController);
                if (typeChanged)
                    m_activeControllerType = type;
                // Set m_lastInputWasController BEFORE emitting signals so QML reads the correct value
                if (!m_lastInputWasController) {
                    m_lastInputWasController = true;
                    emit lastInputWasControllerChanged();
                }
                if (typeChanged)
                    emit controllerTypeChanged();
                auto btn = static_cast<SDL_GameControllerButton>(event.cbutton.button);
                if (btn == SDL_CONTROLLER_BUTTON_BACK) {
                    m_selectHeld = true;
                }
                // Select + Start combo — universal across all
                // controllers. Start is rarely action-mapped in retro
                // games (typically "Pause" or unbound), so the brief
                // press before our SIGSTOP arrives is unlikely to
                // trigger an in-game action.
                if (m_selectHeld && btn == SDL_CONTROLLER_BUTTON_START) {
                    qDebug() << "[SdlInput] Select+Start combo — emitting inGameMenuRequested";
                    emit inGameMenuRequested();
                    break;
                }
                // Touchpad single press: same outcome, no combo.
                // DualShock 4 / DualSense have it; Xbox/Switch don't.
                if (btn == SDL_CONTROLLER_BUTTON_TOUCHPAD) {
                    qDebug() << "[SdlInput] Touchpad press — emitting inGameMenuRequested";
                    emit inGameMenuRequested();
                    break;
                }
                // While the in-game menu panel owns input, suppress
                // signal emits (navigateStart) that fire QML handlers
                // in the *main* window — those would leak past the
                // QML enabled-gate due to binding-update timing. Key
                // injection still goes through (focused window is the
                // panel, so HUD navigation works).
                if (m_suppressMainInputs && btn == SDL_CONTROLLER_BUTTON_START) break;
                // Start emits signal (app-level action, not navigation)
                if (btn == SDL_CONTROLLER_BUTTON_START) {
                    emit navigateStart();
                } else {
                    int qtKey = mapButtonToKey(btn);
                    if (qtKey)
                        injectKey(qtKey, QEvent::KeyPress);
                }
            }
            break;

        case SDL_CONTROLLERBUTTONUP: {
            if (m_capturing && m_multiCapture) {
                emit captureButtonReleased();
            } else if (m_emulationTarget) {
                auto btn = static_cast<SDL_GameControllerButton>(event.cbutton.button);
                if (btn == SDL_CONTROLLER_BUTTON_BACK)
                    m_selectHeld = false;
                {
                    const int devIdx = m_deviceIndices.value(event.cbutton.which, 0);
                    const char* btnName = SDL_GameControllerGetStringForButton(btn);
                    const QString canonical = canonicalName(btnName);
                    const RetroPadSlot slot = m_emulationTarget->lookup(devIdx, canonical);
                    if (slot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, slot, false);
                }
            } else if (!m_capturing) {
                auto btn = static_cast<SDL_GameControllerButton>(event.cbutton.button);
                if (btn == SDL_CONTROLLER_BUTTON_BACK) {
                    m_selectHeld = false;
                }
                int qtKey = mapButtonToKey(btn);
                if (qtKey)
                    injectKey(qtKey, QEvent::KeyRelease);
            }
            break;
        }

        case SDL_CONTROLLERAXISMOTION: {
            int value = event.caxis.value;
            if (m_capturing && std::abs(value) > kAxisDeadzone) {
                int devIdx = m_deviceIndices.value(event.caxis.which, 0);
                const char* axisName = SDL_GameControllerGetStringForAxis(
                    static_cast<SDL_GameControllerAxis>(event.caxis.axis));
                QString element = canonicalName(axisName);
                bool positive = value > 0;
                if (!m_multiCapture) {
                    m_capturing = false;
                    emit capturingChanged();
                }
                emit bindingCaptured(devIdx, element, true, positive);
            } else if (m_emulationTarget && !m_capturing) {
                // Route axis polarity to the InputRouter so analog-mapped-to-
                // RetroPad bindings work (e.g. D-Pad Up = SDL-0/-LeftY).
                // Resolve both "+{axis}" and "-{axis}" via lookup; only the
                // bound polarity gets a press, the opposite gets released.
                const char* axisName = SDL_GameControllerGetStringForAxis(
                    static_cast<SDL_GameControllerAxis>(event.caxis.axis));
                const QString axis = canonicalName(axisName);
                const QString posEl = "+" + axis;
                const QString negEl = "-" + axis;
                const int devIdx = m_deviceIndices.value(event.caxis.which, 0);
                const auto posSlot = m_emulationTarget->lookup(devIdx, posEl);
                const auto negSlot = m_emulationTarget->lookup(devIdx, negEl);

                if (value > kAxisDeadzone) {
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, posSlot, true);
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, negSlot, false);
                } else if (value < -kAxisDeadzone) {
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, negSlot, true);
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, posSlot, false);
                } else if (std::abs(value) <= kAxisDeadzone / 2) {
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, posSlot, false);
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, negSlot, false);
                }
            } else if (!m_capturing && std::abs(value) > kAxisDeadzone) {
                auto axis = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
                SDL_JoystickID jid = event.caxis.which;
                auto key = qMakePair(jid, static_cast<int>(axis));

                if (!m_axisActive.value(key, false)) {
                    m_axisActive[key] = true;
                    auto type = m_controllerTypes.value(event.caxis.which, Xbox);
                    bool typeChanged = (type != m_activeControllerType || !m_lastInputWasController);
                    if (typeChanged)
                        m_activeControllerType = type;
                    if (!m_lastInputWasController) {
                        m_lastInputWasController = true;
                        emit lastInputWasControllerChanged();
                    }
                    if (typeChanged)
                        emit controllerTypeChanged();
                    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
                        injectKey(value > 0 ? Qt::Key_Right : Qt::Key_Left, QEvent::KeyPress);
                    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        injectKey(value > 0 ? Qt::Key_Down : Qt::Key_Up, QEvent::KeyPress);
                    } else if (axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                        emit navigateShift();
                    }
                }
            } else if (!m_capturing && std::abs(value) <= kAxisDeadzone / 2) {
                auto axis = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
                SDL_JoystickID jid = event.caxis.which;
                auto key = qMakePair(jid, static_cast<int>(axis));
                if (m_axisActive.value(key, false)) {
                    m_axisActive[key] = false;
                    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
                        injectKey(Qt::Key_Left, QEvent::KeyRelease);
                        injectKey(Qt::Key_Right, QEvent::KeyRelease);
                    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
                        injectKey(Qt::Key_Up, QEvent::KeyRelease);
                        injectKey(Qt::Key_Down, QEvent::KeyRelease);
                    }
                    // R2 trigger uses signal, no key release needed
                }
            }
            break;
        }

        case SDL_KEYDOWN:
            // Mark that a physical keyboard was used (not controller)
            if (!m_capturing) {
                if (m_lastInputWasController) {
                    m_lastInputWasController = false;
                    emit lastInputWasControllerChanged();
                    emit controllerTypeChanged();
                }
            }
            if (m_capturing) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_LSHIFT || key == SDLK_RSHIFT ||
                    key == SDLK_LCTRL || key == SDLK_RCTRL ||
                    key == SDLK_LALT || key == SDLK_RALT) {
                    break;
                }
                if (key == SDLK_ESCAPE) {
                    cancelCapture();
                    break;
                }
                QString keyStr = keyName(key);
                SDL_Keymod mod = SDL_GetModState();
                QString full;
                if (mod & KMOD_ALT)
                    full = "Keyboard/Alt & Keyboard/" + keyStr;
                else if (mod & KMOD_SHIFT)
                    full = "Keyboard/Shift & Keyboard/" + keyStr;
                else if (mod & KMOD_CTRL)
                    full = "Keyboard/Control & Keyboard/" + keyStr;
                else
                    full = "Keyboard/" + keyStr;

                m_capturing = false;
                emit capturingChanged();
                emit keyboardCaptured(full);
            }
            break;

        default:
            break;
        }
    }
}
