#pragma once
#include <QHash>
#include <QString>
#include <array>
#include <atomic>
#include <QtGlobal>

enum class RetroPadSlot : int {
    None   = -1,
    B      = 0,
    Y      = 1,
    Select = 2,
    Start  = 3,
    Up     = 4,
    Down   = 5,
    Left   = 6,
    Right  = 7,
    A      = 8,
    X      = 9,
    L      = 10,
    R      = 11,
    L2     = 12,
    R2     = 13,
    L3     = 14,
    R3     = 15,
    Count  = 16
};

/**
 * Six analog axes per port. Sticks are paired (LeftX/Y, RightX/Y) and use a
 * radial deadzone (vector magnitude check). Triggers (L2, R2) are 1D and use
 * a per-axis deadzone.
 */
enum class RetroPadAxis : int {
    LeftX  = 0,
    LeftY  = 1,
    RightX = 2,
    RightY = 3,
    L2     = 4,
    R2     = 5,
    Count  = 6
};

/**
 * Map a binding action key string (as stored in controls.ini) to the
 * corresponding RetroPadSlot enum value.  Returns RetroPadSlot::None for
 * unrecognised keys so callers can skip them silently.
 */
inline RetroPadSlot retroPadSlotFromKey(const QString& key) {
    if (key == QStringLiteral("B"))      return RetroPadSlot::B;
    if (key == QStringLiteral("Y"))      return RetroPadSlot::Y;
    if (key == QStringLiteral("Select")) return RetroPadSlot::Select;
    if (key == QStringLiteral("Start"))  return RetroPadSlot::Start;
    if (key == QStringLiteral("Up"))     return RetroPadSlot::Up;
    if (key == QStringLiteral("Down"))   return RetroPadSlot::Down;
    if (key == QStringLiteral("Left"))   return RetroPadSlot::Left;
    if (key == QStringLiteral("Right"))  return RetroPadSlot::Right;
    if (key == QStringLiteral("A"))      return RetroPadSlot::A;
    if (key == QStringLiteral("X"))      return RetroPadSlot::X;
    if (key == QStringLiteral("L"))      return RetroPadSlot::L;
    if (key == QStringLiteral("R"))      return RetroPadSlot::R;
    if (key == QStringLiteral("L2"))     return RetroPadSlot::L2;
    if (key == QStringLiteral("R2"))     return RetroPadSlot::R2;
    if (key == QStringLiteral("L3"))     return RetroPadSlot::L3;
    if (key == QStringLiteral("R3"))     return RetroPadSlot::R3;
    return RetroPadSlot::None;
}

class InputRouter {
public:
    static constexpr int NUM_PORTS = 4;
    static constexpr int NUM_AXES_PER_PORT = static_cast<int>(RetroPadAxis::Count);

    /** Bind: (device index, canonical SDL element name) -> RetroPad slot. */
    void bind(int deviceIdx, const QString& sdlElement, RetroPadSlot slot);
    void clearBindings();

    /** Lookup: returns RetroPadSlot::None if unbound. */
    RetroPadSlot lookup(int deviceIdx, const QString& sdlElement) const;

    void setButtonPressed(int port, RetroPadSlot slot, bool down);
    bool buttonPressed(int port, RetroPadSlot slot) const;

    /**
     * Write raw SDL axis value (-32768..32767) to storage. Called from the
     * Qt/SDL thread. Lock-free atomic store.
     */
    void setAxis(int port, RetroPadAxis axis, int16_t raw);

    /**
     * Read axis value with deadzone applied. Called from the core thread.
     * For LeftX/Y and RightX/Y, applies a radial (vector-magnitude)
     * deadzone using the paired axis. For L2/R2, applies a 1D per-axis
     * deadzone. Returns 0 inside the deadzone, sign-preserving rescaled
     * value outside.
     */
    int16_t axis(int port, RetroPadAxis axis) const;

    /**
     * Set the inner deadzone as a fraction of full scale (0.0..0.5).
     * Default 0.15. Values outside [0.0, 0.5] are clamped. Future RetroNest
     * settings UI will bind here; SP5.5 leaves it at default.
     */
    void setInnerDeadzone(float fraction);

private:
    QHash<QPair<int, QString>, RetroPadSlot> m_bindings;
    std::array<std::atomic<uint32_t>, NUM_PORTS> m_state{};
    std::array<std::atomic<int16_t>, NUM_PORTS * NUM_AXES_PER_PORT> m_axes{};
    std::atomic<float> m_innerDeadzone{0.15f};
};
