#pragma once
#include "libretro_adapter.h"

// Minimal skeleton-phase DuckStationLibretroAdapter.
// coreId() is the only pure-virtual on LibretroAdapter; the registry only
// instantiates concrete subclasses, so we need a named class to register.
// Settings schema, controller types, RA console id, resume lookup, etc. are
// deferred to follow-on specs (see 2026-06-01-duckstation-libretro-skeleton-design.md).
class DuckStationLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "duckstation"; }
    HardwareRenderBackend hardwareRenderBackend() const override {
        return HardwareRenderBackend::MetalNSView;
    }
    // PS1 → rcheevos console id 12, but RA is out of scope for the skeleton;
    // returning 0 keeps rcheevos disabled until the RA sub-spec wires it.
    int raConsoleId(const QString& systemId) const override { return 0; }
};
