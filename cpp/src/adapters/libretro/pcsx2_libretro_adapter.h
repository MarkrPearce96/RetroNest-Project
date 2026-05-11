#pragma once
#include "libretro_adapter.h"

// Minimal skeleton-phase Pcsx2LibretroAdapter.
//
// LibretroAdapter declares coreId() pure-virtual. The registry only
// instantiates concrete subclasses, so even though the skeleton phase
// has no PS2-specific behavior to add, we need a named class to register.
// Later sub-projects (settings schema, RA console ID,
// resume-file lookup, etc.) will fill in the overrides.
class Pcsx2LibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "pcsx2"; }
    bool prefersHardwareRender() const override { return true; }

    // SP5: PS2 DualShock 2 binding defs. Action keys match
    // retroPadSlotFromKey() so the InputRouter resolves bindings on launch.
    // Analog sticks / L2/R2 analog triggers route as digital here (RetroNest's
    // InputRouter is a 16-bit RetroPad bitmask); full analog requires future
    // RetroNest work to extend the router.
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
};
