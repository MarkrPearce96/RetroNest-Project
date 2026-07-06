#pragma once
#include "libretro_adapter.h"

// DuckStationLibretroAdapter — wires RetroNest to the DuckStation libretro
// core (duckstation_libretro.dylib) for PS1: controller types/bindings, the
// settings-schema + Recommended hub, RetroAchievements console ID (PS1=12),
// and cold-resume file lookup.
class DuckStationLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "duckstation"; }
    int maxLibretroPlayers() const override { return 2; }
    HardwareRenderBackend hardwareRenderBackend() const override {
        return HardwareRenderBackend::MetalNSView;
    }
    // PS1 → RC_CONSOLE_PLAYSTATION = 12. Without this, rc_libretro_memory_init
    // fails and achievements never trigger — see Pcsx2LibretroAdapter::raConsoleId.
    int raConsoleId(const QString& systemId) const override {
        return (systemId == "psx") ? 12 : 0;
    }

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
    // States). Mirrors Pcsx2LibretroAdapter::pathsDefs().
    QVector<PathDef> pathsDefs() const override;

    // Resolve the per-game resume save state written by GameSession::terminate
    // under the DuckStation SaveStates dir; without this override the base
    // returns empty, StartConfig.resumeStatePath stays unset, and launching
    // the game cold-boots. Mirrors Pcsx2LibretroAdapter::findResumeFile.
    QString findResumeFile(const QString& serial) const override;

    // Packet 7 Stage 2: schema rendered from the core's declared options
    // (LibretroAdapter::settingsSchema base merge) — this adapter supplies
    // only the curation overlay (UI routing + dependency gates).
    QVector<OptionOverlay> optionOverlays() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QStringList settingsCategoriesWithSubTabs() const override { return {"Graphics"}; }

    // Aspect-ratio preview on the Recommended page (mirrors
    // DolphinLibretroAdapter::previewSpec). No OSD preview yet — Phase 6.
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
};
