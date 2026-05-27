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

    HardwareRenderBackend hardwareRenderBackend() const override {
        // SP2 built the Metal NSView handover path. SP4 will switch to
        // Vulkan when that work lands; until then the core only supports
        // the Metal path on macOS.
        return HardwareRenderBackend::MetalNSView;
    }

    // GameCube = RC_CONSOLE_GAMECUBE (16); Wii = RC_CONSOLE_WII (19).
    // RetroNest's RA console mapping in cpp/src/core/ra_client.cpp already
    // contains the gc/wii string->id entries.
    int raConsoleId(const QString& systemId) const override;

    // SP8: cold-resume lookup. GameSession writes "<serial-or-basename>.resume"
    // under the SaveStates override or emulators/dolphin/<gc|wii>/savestates;
    // Dolphin spans two systems, so the impl searches both (mirrors
    // MgbaLibretroAdapter). Without this override the base returns {} and
    // Save & Quit -> Resume silently no-ops (file written, never read back).
    QString findResumeFile(const QString& key) const override;

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

    // SP6: Graphics core-options schema. Keys + values + defaults mirror
    // dolphin-libretro/Source/Core/DolphinLibretro/CoreOptionsGraphics.cpp's
    // push_back table EXACTLY (enforced by tools/check_schema_fidelity.py).
    QVector<SettingDef> settingsSchema() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QStringList settingsCategoriesWithSubTabs() const override { return {"Graphics"}; }
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;
};
