#include "core/libretro/libretro_hotkey_controller.h"

#include "adapters/libretro/libretro_adapter.h"
#include "core/game_session.h"
#include "core/libretro/audio_sink.h"
#include "core/libretro/core_runtime.h"
#include "core/libretro/hotkey_dispatcher.h"
#include "core/libretro/hotkey_matcher.h"
#include "core/sdl_input_manager.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QVariantMap>

LibretroHotkeyController::LibretroHotkeyController(SessionProvider sessionProvider,
                                                   SuppressionCheck extraSuppression,
                                                   QObject* parent)
    : QObject(parent),
      m_session(std::move(sessionProvider)),
      m_extraSuppression(std::move(extraSuppression)) {
    m_matcher = std::make_unique<HotkeyMatcher>();

    // Action bodies — every one routes through the session provider, so
    // they harmlessly no-op when no libretro game is running.
    HotkeyDispatcher::Callbacks cb;
    cb.saveStateSlot   = [this](int s) {
        if (auto* gs = m_session()) gs->saveStateLibretro(s);
    };
    cb.loadStateSlot   = [this](int s) {
        if (auto* gs = m_session()) gs->loadStateLibretro(s);
    };
    cb.getCurrentSlot  = [this]() -> int {
        if (auto* gs = m_session()) return gs->currentSaveSlot();
        return 1;
    };
    cb.setCurrentSlot  = [this](int s) {
        if (auto* gs = m_session()) {
            gs->setCurrentSaveSlot(s);  // GameSession clamps to [1,5] internally
            emit infoToastRequested(QStringLiteral("Save State"),
                                    QStringLiteral("Slot %1").arg(gs->currentSaveSlot()),
                                    QString(), QString(), 1500);
        }
    };
    cb.toggleFastForward = [this]() {
        if (auto* gs = m_session()) gs->toggleFastForwardLibretro();
    };
    cb.setFastForward = [this](bool on) {
        // Hold-style action. GameSession only exposes toggleFastForwardLibretro
        // today; the matcher's hold semantics call this once on each edge, so
        // toggling on both lands at most twice per hold.
        if (auto* gs = m_session()) {
            Q_UNUSED(on);
            gs->toggleFastForwardLibretro();
        }
    };
    cb.togglePause = [this]() {
        if (auto* gs = m_session()) {
            auto* la = gs->adapter() ? gs->adapter()->asLibretro() : nullptr;
            if (la && la->runtime()) {
                if (la->runtime()->isPaused()) la->runtime()->resume();
                else la->runtime()->pause();
            }
        }
    };
    cb.reset = [this]() {
        if (auto* gs = m_session()) {
            auto* la = gs->adapter() ? gs->adapter()->asLibretro() : nullptr;
            if (la && la->runtime()) la->runtime()->reset();
        }
    };
    cb.openMenu = [this]() {
        // QML handles this by calling toggleInGameMenu, which selects the
        // libretro-aware menu (overlay / in-scene HUD).
        emit menuToggleRequested();
    };
    cb.toggleMute = [this]() {
        if (auto* gs = m_session()) {
            auto* la = gs->adapter() ? gs->adapter()->asLibretro() : nullptr;
            if (la && la->runtime() && la->runtime()->audioSink()) {
                auto* a = la->runtime()->audioSink();
                a->setMuted(!a->isMuted());
            }
        }
    };
    cb.adjustVolume = [this](int dPct) {
        if (auto* gs = m_session()) {
            auto* la = gs->adapter() ? gs->adapter()->asLibretro() : nullptr;
            if (la && la->runtime() && la->runtime()->audioSink()) {
                auto* a = la->runtime()->audioSink();
                a->setVolume(a->volume() + dPct / 100.0f);
            }
        }
    };

    m_dispatcher = std::make_unique<HotkeyDispatcher>(std::move(cb));
    connect(m_matcher.get(), &HotkeyMatcher::actionPressed,
            m_dispatcher.get(), &HotkeyDispatcher::onActionPressed);
    connect(m_matcher.get(), &HotkeyMatcher::actionReleased,
            m_dispatcher.get(), &HotkeyDispatcher::onActionReleased);

    if (auto* app = QCoreApplication::instance())
        app->installEventFilter(this);
}

LibretroHotkeyController::~LibretroHotkeyController() = default;

void LibretroHotkeyController::setBindings(const QVariantList& rows) {
    m_matcher->clearAllBindings();
    for (const QVariant& v : rows) {
        const QVariantMap m = v.toMap();
        m_matcher->setBinding(m.value(QStringLiteral("key")).toString(),
                              m.value(QStringLiteral("currentValue")).toString());
    }
}

void LibretroHotkeyController::setUiSuppressed(bool suppressed) {
    m_uiSuppressed = suppressed;
}

void LibretroHotkeyController::acquireSuppression() {
    ++m_suppressCount;
}

void LibretroHotkeyController::releaseSuppression() {
    if (m_suppressCount > 0) --m_suppressCount;
}

bool LibretroHotkeyController::suppressed() const {
    return m_uiSuppressed
           || m_suppressCount > 0
           || (m_extraSuppression && m_extraSuppression());
}

void LibretroHotkeyController::attachInputManager(SdlInputManager* mgr) {
    if (!mgr) return;
    connect(mgr, &SdlInputManager::gamepadButtonChanged,
            this, [this](int port, int btn, bool pressed) {
                m_matcher->onGamepadButton(port, btn, pressed);
            });
}

void LibretroHotkeyController::resetGamepadState() {
    m_matcher->resetGamepadState();
}

bool LibretroHotkeyController::handleKeyEvent(int combinedKey, bool pressed) {
    if (suppressed()) return false;
    return m_matcher->onKeyEvent(combinedKey, pressed);
}

bool LibretroHotkeyController::eventFilter(QObject* /*watched*/, QEvent* event) {
    const QEvent::Type t = event->type();
    if (t == QEvent::KeyPress || t == QEvent::KeyRelease) {
        auto* k = static_cast<QKeyEvent*>(event);
        // Suppress hotkey dispatch when a host UI surface owns the key event
        // (QML modal policy, a widget dialog's suppression hold, or the
        // injected extra check — see class docs).
        if (!k->isAutoRepeat()) {
            const int combined = int(k->key()) | int(k->modifiers());
            if (handleKeyEvent(combined, t == QEvent::KeyPress)) {
                // A bound action fired. Consume the event so it doesn't
                // also reach the just-opened menu's own Esc-to-close
                // handler (or any other downstream widget shortcut).
                event->accept();
                return true;
            }
        }
    }
    return false;  // unbound key — keep Qt's normal routing intact
}
