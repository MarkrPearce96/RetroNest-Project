#include "input_router.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

void InputRouter::bind(int deviceIdx, const QString& sdlElement, RetroPadSlot slot) {
    m_bindings[{deviceIdx, sdlElement}] = slot;
}

void InputRouter::clearBindings() { m_bindings.clear(); }

RetroPadSlot InputRouter::lookup(int deviceIdx, const QString& sdlElement) const {
    auto it = m_bindings.constFind({deviceIdx, sdlElement});
    return (it == m_bindings.constEnd()) ? RetroPadSlot::None : it.value();
}

void InputRouter::setButtonPressed(int port, RetroPadSlot slot, bool down) {
    if (port < 0 || port >= NUM_PORTS || slot == RetroPadSlot::None) return;
    uint32_t bit = 1u << static_cast<int>(slot);
    auto& s = m_state[port];
    if (down) s.fetch_or(bit, std::memory_order_relaxed);
    else      s.fetch_and(~bit, std::memory_order_relaxed);
}

bool InputRouter::buttonPressed(int port, RetroPadSlot slot) const {
    if (port < 0 || port >= NUM_PORTS || slot == RetroPadSlot::None) return false;
    uint32_t bit = 1u << static_cast<int>(slot);
    return (m_state[port].load(std::memory_order_relaxed) & bit) != 0;
}

void InputRouter::setAxis(int port, RetroPadAxis axis, int16_t raw) {
    if (port < 0 || port >= NUM_PORTS) return;
    const int idx = static_cast<int>(axis);
    if (idx < 0 || idx >= NUM_AXES_PER_PORT) return;
    m_axes[port * NUM_AXES_PER_PORT + idx].store(raw, std::memory_order_relaxed);
}

void InputRouter::setInnerDeadzone(float fraction) {
    fraction = std::clamp(fraction, 0.0f, 0.5f);
    m_innerDeadzone.store(fraction, std::memory_order_relaxed);
}

int16_t InputRouter::axis(int port, RetroPadAxis axis) const {
    if (port < 0 || port >= NUM_PORTS) return 0;
    const int idx = static_cast<int>(axis);
    if (idx < 0 || idx >= NUM_AXES_PER_PORT) return 0;

    const int16_t raw =
        m_axes[port * NUM_AXES_PER_PORT + idx].load(std::memory_order_relaxed);
    const float dzFrac = m_innerDeadzone.load(std::memory_order_relaxed);
    const float dz = dzFrac * 32767.0f;

    // Triggers: per-axis 1D deadzone.
    if (axis == RetroPadAxis::L2 || axis == RetroPadAxis::R2) {
        const float fabsRaw = std::fabs(static_cast<float>(raw));
        if (fabsRaw < dz) return 0;
        // Sign-preserving rescale: (|raw| - dz) / (32767 - dz) * 32767.
        const float denom = 32767.0f - dz;
        if (denom <= 0.0f) return raw;  // safety: dz clamp prevents this in practice
        const float scaled = (fabsRaw - dz) / denom * 32767.0f;
        return static_cast<int16_t>(raw < 0 ? -scaled : scaled);
    }

    // Sticks: radial deadzone using the paired axis.
    RetroPadAxis pair;
    switch (axis) {
        case RetroPadAxis::LeftX:  pair = RetroPadAxis::LeftY;  break;
        case RetroPadAxis::LeftY:  pair = RetroPadAxis::LeftX;  break;
        case RetroPadAxis::RightX: pair = RetroPadAxis::RightY; break;
        case RetroPadAxis::RightY: pair = RetroPadAxis::RightX; break;
        default: return 0;
    }
    const int16_t other =
        m_axes[port * NUM_AXES_PER_PORT + static_cast<int>(pair)]
            .load(std::memory_order_relaxed);

    const float rawF   = static_cast<float>(raw);
    const float otherF = static_cast<float>(other);
    const float mag = std::sqrt(rawF * rawF + otherF * otherF);
    if (mag < dz) return 0;

    const float denom = 32767.0f - dz;
    if (denom <= 0.0f) return raw;
    const float scaledMag = (mag - dz) / denom * 32767.0f;
    // Scale this component proportionally: raw * (scaledMag / mag).
    const float comp = rawF * (scaledMag / mag);
    // Clamp to int16 range (just in case of fp rounding past 32767).
    if (comp >  32767.0f) return  32767;
    if (comp < -32768.0f) return -32768;
    return static_cast<int16_t>(comp);
}
