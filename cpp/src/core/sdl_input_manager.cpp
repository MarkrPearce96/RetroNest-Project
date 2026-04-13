#include "sdl_input_manager.h"
#include <QDebug>
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
    if (m_window)
        m_window->installEventFilter(this);
}

bool SdlInputManager::eventFilter(QObject* obj, QEvent* event) {
    // Detect real keyboard presses (not injected by us) to switch back to keyboard mode
    if (event->type() == QEvent::KeyPress && m_lastInputWasController &&
        !m_capturing && !m_injectingKey) {
        m_lastInputWasController = false;
        emit lastInputWasControllerChanged();
        emit controllerTypeChanged();
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

QString SdlInputManager::canonicalName(const char* sdlName) {
    if (!sdlName) return {};
    return canonicalMap().value(QString(sdlName).toLower(), QString(sdlName));
}

void SdlInputManager::injectKey(int qtKey, QEvent::Type type) {
    if (!m_window) return;
    m_injectingKey = true;
    if (!m_lastInputWasController) {
        m_lastInputWasController = true;
        emit lastInputWasControllerChanged();
    }
    QKeyEvent event(type, qtKey, Qt::NoModifier);
    QGuiApplication::sendEvent(m_window, &event);
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
                // Select + B/Circle combo detection for in-game menu
                if (btn == SDL_CONTROLLER_BUTTON_BACK) {
                    m_selectHeld = true;
                }
                if (m_selectHeld && btn == SDL_CONTROLLER_BUTTON_B) {
                    qInfo() << "[SdlInput] Select+B combo detected — emitting inGameMenuRequested";
                    emit inGameMenuRequested();
                    break; // Don't inject B as Key_Back when used in combo
                }
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
            } else if (!m_capturing) {
                auto btn = static_cast<SDL_GameControllerButton>(event.cbutton.button);
                // Track Select/Back release for combo detection
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
