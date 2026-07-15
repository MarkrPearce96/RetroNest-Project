#pragma once
#include "libretro_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// DolphinLibretroAdapter — wires RetroNest to the Dolphin libretro core
// (dolphin_libretro.dylib) for GameCube + Wii: controller types/bindings,
// the settings-schema + Recommended hub, RetroAchievements console IDs
// (GC=16 / Wii=19), and cold-resume file lookup.
class DolphinLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "dolphin"; }

    // Quick-settings tabs (Resolution / Aspect Ratio) → core options + a
    // curated pill shortlist (full value set stays on the main settings page).
    QString resolutionOptionKey() const override { return "dolphin_internal_resolution"; }
    QString aspectRatioOptionKey() const override { return "dolphin_aspect_ratio"; }
    QVector<QPair<QString, QString>> resolutionOptionShortlist() const override {
        return {{"1x", "1x"}, {"2x", "2x"}, {"4x", "4x"}, {"8x", "8x"}};
    }
    QVector<QPair<QString, QString>> aspectRatioOptionShortlist() const override {
        return {{"Auto", "Auto"}, {"4:3", "4:3"}, {"16:9", "16:9"}};
    }
    // 16:9 → enable the widescreen hack (enabled/disabled), else disable.
    QString widescreenOptionKey() const override { return "dolphin_widescreen_hack"; }

    HardwareRenderBackend hardwareRenderBackend() const override {
        // SP2 built the Metal NSView handover path. SP4 will switch to
        // Vulkan when that work lands; until then the core only supports
        // the Metal path on macOS.
        return HardwareRenderBackend::MetalNSView;
    }

    // SP8: cold-resume lookup. GameSession writes "<serial-or-basename>.resume"
    // under the SaveStates override or emulators/dolphin/<gc|wii>/savestates;
    // Dolphin spans two systems, so the impl searches both (mirrors
    // MgbaLibretroAdapter). Without this override the base returns {} and
    // Save & Quit -> Resume silently no-ops (file written, never read back).
    QString findResumeFile(const QString& key) const override;

    // Paths settings: expose the Save States override, applied generically via
    // GameSession + findResumeFile above. Mirrors the other libretro adapters.
    QVector<PathDef> pathsDefs() const override;

    // GameCube/Wii discs store a 6-char game ID (e.g. "GZ2P01") at disc offset 0.
    // The base extractSerial() reads PlayStation's SYSTEM.CNF, so it fails on GC
    // discs and can't read Dolphin's compressed .rvz/.wia at all. Read the game
    // ID directly: at file offset 0 for raw .iso/.gcm, and from the verbatim
    // disc_header in the WIA/RVZ header for .rvz/.wia.
    QString extractSerial(const QString& romPath) const override;

    // SP5: surface GameCube + Wii Classic controllers to the mapping UI.
    // Without controllerTypes() the page is empty and ControllerBindingsView
    // crashes rendering cards (see Pcsx2LibretroAdapter note).
    QVector<ControllerTypeDef> controllerTypes() const override;

    // SP5: digital binding defs. Keys are RetroPad slots resolved by
    // retroPadSlotFromKey(); only the slots RetroNest seeds (A/B/X/Y, L/R,
    // Select/Start, D-Pad) get a default physical binding. Analog sticks are
    // fixed-routed by SdlInputManager (not controls.ini-remappable), so they
    // are intentionally absent here — they still drive the game in-core via
    // the GCPad profile the libretro core writes at boot.
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;

    // SP5: seed a fresh controls.ini from each binding def's defaultValue rather
    // than the base class's shared hardcoded slot->element map (which would seed
    // GC L/R as shoulders, not analog triggers). Makes the fresh-file defaults
    // match what Auto-Map produces (auto-map already uses defaultValue).
    bool ensureConfig(const EmulatorManifest& manifest,
                      const QString& biosPath,
                      const QString& savesPath) override;

    // Packet 7 Stage 2: schema renders from the core's declared options
    // (LibretroAdapter::settingsSchema base merge) — this adapter supplies
    // routing/curation only (all rows are core options; no extraSettings).
    QVector<OptionOverlay> optionOverlays() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QStringList settingsCategoriesWithSubTabs() const override { return {"Graphics"}; }
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
};
