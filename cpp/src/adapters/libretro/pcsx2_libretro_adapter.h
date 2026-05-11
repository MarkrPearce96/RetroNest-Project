#pragma once
#include "libretro_adapter.h"

// Minimal skeleton-phase Pcsx2LibretroAdapter.
//
// LibretroAdapter declares coreId() pure-virtual. The registry only
// instantiates concrete subclasses, so even though the skeleton phase
// has no PS2-specific behavior to add, we need a named class to register.
// Later sub-projects (controller mapping, settings schema, RA console ID,
// resume-file lookup, etc.) will fill in the overrides.
class Pcsx2LibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "pcsx2"; }
    bool prefersHardwareRender() const override { return true; }
};
