#include "sdl_input_manager.h"
#include "libretro.h"   // retro_rumble_effect, RETRO_RUMBLE_STRONG/WEAK
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
#include <QSet>
#include <QVariantMap>
#include <cmath>
#include <cstdlib>

// Deadzone threshold for axis capture (out of 32767)
static constexpr int kAxisDeadzone = 16000;

static bool inputTraceEnabled() {
    static const bool v = (std::getenv("RETRONEST_INPUT_TRACE") != nullptr);
    return v;
}

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
    // SP5.5: enable HIDAPI drivers explicitly before SDL_Init so the PS5
    // driver (which handles DualSense rumble + battery + lightbar) is
    // active when it's built into the SDL2 library. SDL2 builds normally
    // default this to enabled, but some package-manager builds may have
    // it off, and SDL_SetHint must run before SDL_Init for these hints to
    // take effect. SDL_HINT_JOYSTICK_HIDAPI is the master switch;
    // SDL_HINT_JOYSTICK_HIDAPI_PS5 specifically controls DualSense.
    // Without HIDAPI PS5 active, SDL_GameControllerRumble returns -1
    // ("That operation is not supported") for the DualSense.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");

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
            // Escape is bindable like any other key — page-level eventFilter
            // tracks press/release so capture commits when the user lets go.
            // To cancel capture, right-click the row or wait for timer expiry.

            // Build a "Keyboard/<name>" string matching the format
            // emitted by the SDL_KEYDOWN path.
            const QString keyStr = qtKeyToCanonicalName(key);
            const auto mods = ke->modifiers();
            QString full;
            if      (mods & Qt::AltModifier)     full = "Keyboard/Alt & Keyboard/"     + keyStr;
            else if (mods & Qt::ShiftModifier)   full = "Keyboard/Shift & Keyboard/"   + keyStr;
            else if (mods & Qt::ControlModifier) full = "Keyboard/Control & Keyboard/" + keyStr;
            else                                 full = "Keyboard/" + keyStr;

            emit keyboardCaptured(full);
            // Do NOT set m_capturing=false or return true: the libretro
            // GenericHotkeyPage tracks every press via its OWN eventFilter
            // (hold-to-multi-bind / tap-to-single-bind), and that filter
            // never fires if we consume the event here. Pages that want
            // single-shot keyboard capture (controller_mapping_page) act
            // on the keyboardCaptured signal — they don't depend on the
            // event being consumed at qApp level.
            return false;
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

QList<int> SdlInputManager::connectedDeviceIndices() const {
    std::lock_guard<std::mutex> lock(m_controllerMx);
    QList<int> indices;
    for (auto it = m_deviceIndices.constBegin(); it != m_deviceIndices.constEnd(); ++it)
        indices.append(it.value());
    return indices;
}

void SdlInputManager::startCapture() {
    m_capturing = true;
    m_captureAxisHeld.clear();
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

// Map SDL_GameControllerAxis -> RetroPadAxis. Returns RetroPadAxis::Count
// for SDL axes we don't surface as analog (currently none — all 6 SDL axes
// map onto our enum).
static RetroPadAxis sdlAxisToRetroPadAxis(SDL_GameControllerAxis a) {
    switch (a) {
        case SDL_CONTROLLER_AXIS_LEFTX:        return RetroPadAxis::LeftX;
        case SDL_CONTROLLER_AXIS_LEFTY:        return RetroPadAxis::LeftY;
        case SDL_CONTROLLER_AXIS_RIGHTX:       return RetroPadAxis::RightX;
        case SDL_CONTROLLER_AXIS_RIGHTY:       return RetroPadAxis::RightY;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:  return RetroPadAxis::L2;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return RetroPadAxis::R2;
        default:                               return RetroPadAxis::Count;
    }
}

void SdlInputManager::setEmulationMode(InputRouter* target) {
    m_emulationTarget = target;
    qInfo() << "[SdlInput] Emulation mode ON — routing via InputRouter";
}

void SdlInputManager::clearEmulationMode() {
    m_emulationTarget = nullptr;
    qInfo() << "[SdlInput] Emulation mode OFF";
}

bool SdlInputManager::setRumbleMotor(int port, unsigned motor,
                                     uint16_t strength) {
    if (port < 0 || port >= InputRouter::NUM_PORTS) return false;

    if (motor == static_cast<unsigned>(RETRO_RUMBLE_STRONG))
        m_rumbleCache[port].low.store(strength, std::memory_order_relaxed);
    else if (motor == static_cast<unsigned>(RETRO_RUMBLE_WEAK))
        m_rumbleCache[port].high.store(strength, std::memory_order_relaxed);
    else
        return false;

    // Lock the QMap reads + the SDL_GameControllerRumble call together:
    // the controller pointer's lifetime is tied to closeController on the
    // Qt thread, which holds the same mutex around its SDL_GameControllerClose.
    std::lock_guard<std::mutex> lock(m_controllerMx);

    SDL_JoystickID jid = -1;
    for (auto it = m_deviceIndices.constBegin(); it != m_deviceIndices.constEnd(); ++it) {
        if (it.value() == port) { jid = it.key(); break; }
    }
    if (jid < 0) return false;

    SDL_GameController* ctrl = m_controllers.value(jid, nullptr);
    if (!ctrl) return false;

    const uint16_t low  = m_rumbleCache[port].low.load(std::memory_order_relaxed);
    const uint16_t high = m_rumbleCache[port].high.load(std::memory_order_relaxed);
    if (inputTraceEnabled()) {
        qDebug("[rumble] port=%d low=%u high=%u", port, low, high);
    }
    SDL_GameControllerRumble(ctrl, low, high, kRumbleDurationMs);
    return true;
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

    // Lock for the QMap mutations — paired with setRumbleMotor's lock so the
    // core thread can never observe a half-updated map.
    std::lock_guard<std::mutex> lock(m_controllerMx);

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
    // Lock for the full duration: paired with setRumbleMotor's lock so the
    // core thread cannot call SDL_GameControllerRumble on a handle we are
    // mid-closing. The mutex covers the stop-rumble + close + map remove.
    std::lock_guard<std::mutex> lock(m_controllerMx);

    if (auto* ctrl = m_controllers.value(instanceId, nullptr)) {
        qInfo() << "[SDL] Controller disconnected:" << SDL_GameControllerName(ctrl);
        const int port = m_deviceIndices.value(instanceId, -1);
        if (port >= 0 && port < InputRouter::NUM_PORTS) {
            // If this controller had rumble active, stop it before closing the
            // SDL handle — otherwise the motors keep running until SDL's
            // duration expires (or the OS scrubs them on close, which is
            // platform-dependent).
            SDL_GameControllerRumble(ctrl, 0, 0, 0);
            m_rumbleCache[port].low.store(0, std::memory_order_relaxed);
            m_rumbleCache[port].high.store(0, std::memory_order_relaxed);
            // Zero this device's axes so straggler axis events don't fall
            // back to port=0 and write phantom P1 input.
            if (m_emulationTarget) {
                for (int ax = 0; ax < static_cast<int>(RetroPadAxis::Count); ++ax)
                    m_emulationTarget->setAxis(port, static_cast<RetroPadAxis>(ax), 0);
            }
        }
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
                    if (slot != RetroPadSlot::None) {
                        m_emulationTarget->setButtonPressed(devIdx, slot, true);
                        // Surface the libretro-index edge to the hotkey matcher
                        // (AppController). Single emission per real SDL edge —
                        // m_emulationTarget gating + the m_capturing branch
                        // above keep this from firing during rebinding UI.
                        emit gamepadButtonChanged(devIdx, static_cast<int>(slot), true);
                    }
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
                // press before the pause lands is unlikely to trigger
                // an in-game action.
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
                // Start emits signal (app-level action, not navigation).
                // Deferred ~50 ms so the combo-detection has a chance
                // when the user fires Select+Start but the controller
                // happens to report Start first (HID-order race). If
                // Select press lands within the window, m_selectHeld
                // flips true and we cancel the navigateStart.
                if (btn == SDL_CONTROLLER_BUTTON_START) {
                    QTimer::singleShot(50, this, [this]() {
                        if (m_selectHeld) return;            // combo will fire
                        emit navigateStart();
                    });
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
                    if (slot != RetroPadSlot::None) {
                        m_emulationTarget->setButtonPressed(devIdx, slot, false);
                        emit gamepadButtonChanged(devIdx, static_cast<int>(slot), false);
                    }
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
            if (m_capturing) {
                // Capture triggers/sticks as press/release EDGES so multi-
                // capture combos commit on release like digital buttons. A
                // trigger release is an axis event (never SDL_CONTROLLERBUTTONUP),
                // so without an explicit release edge the combo stays stuck in
                // binding mode.
                const auto axisKey = qMakePair(event.caxis.which,
                                               static_cast<int>(event.caxis.axis));
                if (std::abs(value) > kAxisDeadzone) {
                    if (!m_captureAxisHeld.value(axisKey, false)) {
                        m_captureAxisHeld[axisKey] = true;
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
                    }
                } else if (std::abs(value) <= kAxisDeadzone / 2) {
                    if (m_captureAxisHeld.remove(axisKey) > 0 && m_multiCapture)
                        emit captureButtonReleased();
                }
            } else if (m_emulationTarget) {
                // Two-fold routing on every axis event:
                //  1. Existing: '+axis'/'-axis' bindings write digital presses
                //     into the InputRouter bitmask. Keeps D-Pad-bound-to-stick
                //     and other digital-emulation bindings working.
                //  2. SP5.5: write the raw int16 magnitude into the axis
                //     storage. PCSX2's analog bindings ("-LeftY", "+L2", ...)
                //     query this via RETRO_DEVICE_ANALOG.
                const auto sdlAxis = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
                const char* axisName = SDL_GameControllerGetStringForAxis(sdlAxis);
                const QString axis = canonicalName(axisName);
                const QString posEl = "+" + axis;
                const QString negEl = "-" + axis;
                const int devIdx = m_deviceIndices.value(event.caxis.which, 0);
                const auto posSlot = m_emulationTarget->lookup(devIdx, posEl);
                const auto negSlot = m_emulationTarget->lookup(devIdx, negEl);

                // (1) Digital emulation. Set the router bit AND surface the
                // press/release EDGE to the hotkey matcher (gamepadButtonChanged),
                // so triggers (L2/R2, which arrive only as axes) and other
                // axis→slot bindings can drive hotkeys exactly like digital
                // buttons. Edge-detected against the router's prior state so
                // continuous axis motion emits at most one press + one release.
                auto setSlotEdge = [&](RetroPadSlot slot, bool pressed) {
                    if (slot == RetroPadSlot::None) return;
                    const bool was = m_emulationTarget->buttonPressed(devIdx, slot);
                    m_emulationTarget->setButtonPressed(devIdx, slot, pressed);
                    if (was != pressed)
                        emit gamepadButtonChanged(devIdx, static_cast<int>(slot), pressed);
                };
                if (value > kAxisDeadzone) {
                    setSlotEdge(posSlot, true);
                    setSlotEdge(negSlot, false);
                } else if (value < -kAxisDeadzone) {
                    setSlotEdge(negSlot, true);
                    setSlotEdge(posSlot, false);
                } else if (std::abs(value) <= kAxisDeadzone / 2) {
                    setSlotEdge(posSlot, false);
                    setSlotEdge(negSlot, false);
                }

                // (2) Analog magnitude — raw int16, deadzone applied at read time.
                const RetroPadAxis rpAxis = sdlAxisToRetroPadAxis(sdlAxis);
                if (rpAxis != RetroPadAxis::Count) {
                    m_emulationTarget->setAxis(devIdx, rpAxis,
                                               static_cast<int16_t>(value));
                    if (inputTraceEnabled()) {
                        qDebug("[sdl] port=%d axis=%d raw=%d", devIdx,
                               static_cast<int>(rpAxis), value);
                    }
                }
            } else if (std::abs(value) > kAxisDeadzone) {
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
            } else if (std::abs(value) <= kAxisDeadzone / 2) {
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
