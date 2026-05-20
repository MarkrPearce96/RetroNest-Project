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

    // PS2 → rcheevos console ID 21 (RC_CONSOLE_PLAYSTATION_2).
    // Without this override, the base returns 0 (UNKNOWN) →
    // rc_libretro_memory_init fails → cheevo set loads with regions=0
    // and achievements never trigger. Discovered during SP6 smoke
    // testing (the SP6 spec assumed RetroNest side was fully wired —
    // the adapter override was missing for PCSX2 specifically).
    int raConsoleId(const QString& systemId) const override {
        return (systemId == "ps2") ? 21 : 0;
    }

    // SP8: PS2 DualShock 2 is the only controller type RetroNest exposes for
    // PCSX2 — matches the deleted standalone adapter's surface. Without
    // this override the base returns {}, the controller-mapping page is
    // empty, and ControllerBindingsView crashes when rendering cards.
    QVector<ControllerTypeDef> controllerTypes() const override {
        return {
            {"DualShock2", "DualShock 2",
             ":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg"},
        };
    }

    // SP5: PS2 DualShock 2 binding defs. Action keys match
    // retroPadSlotFromKey() so the InputRouter resolves bindings on launch.
    // Analog sticks / L2/R2 analog triggers route as digital here (RetroNest's
    // InputRouter is a 16-bit RetroPad bitmask); full analog requires future
    // RetroNest work to extend the router.
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;

    // Path overrides: three user-overridable folders (Memory Cards, Save
    // States, Textures). Listed here rather than next to biosFiles() because
    // PCSX2's BIOS is shared globally — see Pcsx2LibretroAdapter::pathsDefs()
    // comment in .cpp.
    QVector<PathDef> pathsDefs() const override;

    // SP6.5: resolve the per-game resume save state written by
    // GameSession::terminate (cpp/src/core/game_session.cpp:392) on its way
    // into retro_unload_game. Without this override the base class returns
    // empty, GameSession's StartConfig.resumeStatePath stays unset, and
    // launching the game cold-boots through BIOS even when a resume file
    // exists on disk. Mirrors MgbaLibretroAdapter::findResumeFile.
    QString findResumeFile(const QString& serial) const override;

    // SP7b: declare libretro core options as user-tweakable rows in the
    // per-emulator settings dialog. Three knobs (renderer / MTVU / FastBoot)
    // are exposed; values mirror pcsx2-libretro/CoreOptions.cpp's
    // kDefinitions[] table exactly so OptionsStore::load's whitelist check
    // accepts persisted values.
    QVector<SettingDef> settingsSchema() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QStringList settingsCategoriesWithSubTabs() const override { return {"Graphics"}; }

    // SP7c Phase 5 — preview widgets for Recommended (aspect) + Graphics
    // On-Screen Display (osd). Returns empty PreviewSpec for every other
    // (category, subcategory) — GenericSettingsPage falls back to no-pane.
    PreviewSpec previewSpec(const QString& category,
                             const QString& subcategory) const override;
};
