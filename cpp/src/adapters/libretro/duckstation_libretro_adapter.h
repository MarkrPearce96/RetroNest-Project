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

    // PS1 Digital Controller is the controller type RetroNest exposes for
    // DuckStation. Without this override the base returns {}, the
    // controller-mapping page is empty, the InputRouter has no bindings to
    // resolve, and CoreRuntime's input trampoline always reports 0 → "no
    // controller input". Mirrors Pcsx2LibretroAdapter.
    QVector<ControllerTypeDef> controllerTypes() const override;

    // PS1 digital pad binding defs. Action keys (the .key field) match
    // retroPadSlotFromKey() (input_router.h) so GameSession's controls.ini
    // parser resolves each line to a RetroPadSlot and binds it into the
    // InputRouter. The PS1 RetroPad layout is identical to PS2's, so the
    // slots + default SDL bindings mirror Pcsx2LibretroAdapter exactly; only
    // the labels differ (PS1 button names). A digital pad has no analog
    // sticks, so L3/R3 are omitted.
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;

    // Path overrides: two user-overridable folders (Memory Cards, Save
    // States). DuckStation has no Textures PathDef in this skeleton, unlike
    // pcsx2's three. Mirrors Pcsx2LibretroAdapter::pathsDefs().
    QVector<PathDef> pathsDefs() const override;

    // Resolve the per-game resume save state written by GameSession::terminate
    // under the DuckStation SaveStates dir; without this override the base
    // returns empty, StartConfig.resumeStatePath stays unset, and launching
    // the game cold-boots. Mirrors Pcsx2LibretroAdapter::findResumeFile.
    QString findResumeFile(const QString& serial) const override;
};
