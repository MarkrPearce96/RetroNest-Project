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

    // Quick-settings tabs (Resolution / Aspect Ratio) → core options + a
    // curated pill shortlist (full value set stays on the main settings page).
    QString resolutionOptionKey() const override { return "pcsx2_upscale_multiplier"; }
    QString aspectRatioOptionKey() const override { return "pcsx2_aspect_ratio"; }
    QVector<QPair<QString, QString>> resolutionOptionShortlist() const override {
        return {{"1", "1x"}, {"2", "2x"}, {"4", "4x"}, {"8", "8x"}};
    }
    QVector<QPair<QString, QString>> aspectRatioOptionShortlist() const override {
        return {{"Auto 4:3/3:2", "Auto"}, {"4:3", "4:3"}, {"16:9", "16:9"}};
    }
    // 16:9 → enable widescreen patches (enabled/disabled), else disable.
    QString widescreenOptionKey() const override { return "pcsx2_enable_widescreen_patches"; }
    HardwareRenderBackend hardwareRenderBackend() const override {
        return HardwareRenderBackend::MetalNSView;
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

    // Packet 7 Stage 2: schema renders from the core's declared options
    // (LibretroAdapter::settingsSchema base merge) — this adapter supplies
    // routing/curation only: placements, 27 dependsOn gates, and the three
    // deliberate RetroNest default overrides (upscale 2x / 16:9 /
    // widescreen patches on). All rows are core options; no extraSettings.
    QVector<OptionOverlay> optionOverlays() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QStringList settingsCategoriesWithSubTabs() const override { return {"Graphics"}; }

    // SP7c Phase 5 — preview widgets for Recommended (aspect) + Graphics
    // On-Screen Display (osd). Returns empty PreviewSpec for every other
    // (category, subcategory) — GenericSettingsPage falls back to no-pane.
    PreviewSpec previewSpec(const QString& category,
                             const QString& subcategory) const override;
};
