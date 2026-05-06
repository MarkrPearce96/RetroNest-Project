#pragma once
#include <QHash>
#include <QString>
#include <array>
#include <atomic>

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

class InputRouter {
public:
    static constexpr int NUM_PORTS = 4;

    /** Bind: (device index, canonical SDL element name) -> RetroPad slot. */
    void bind(int deviceIdx, const QString& sdlElement, RetroPadSlot slot);
    void clearBindings();

    /** Lookup: returns RetroPadSlot::None if unbound. */
    RetroPadSlot lookup(int deviceIdx, const QString& sdlElement) const;

    void setButtonPressed(int port, RetroPadSlot slot, bool down);
    bool buttonPressed(int port, RetroPadSlot slot) const;

private:
    QHash<QPair<int, QString>, RetroPadSlot> m_bindings;
    std::array<std::atomic<uint32_t>, NUM_PORTS> m_state{};
};
