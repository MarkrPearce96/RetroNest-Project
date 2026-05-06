#include "input_router.h"

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
