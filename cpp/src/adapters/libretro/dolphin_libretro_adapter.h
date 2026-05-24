#pragma once
#include "libretro_adapter.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"

// Skeleton-phase DolphinLibretroAdapter.
//
// LibretroAdapter declares coreId() pure-virtual; the registry only
// instantiates concrete subclasses, so even though SP3 ships a minimal
// surface (most overrides return empty), we need a named class to
// register. SP4 (Vulkan path), SP5 (controllers), SP6/7 (settings), and
// SP8 (achievements + polish) fill in the remaining overrides
// incrementally.
//
// Replaces the standalone DolphinAdapter that previously launched
// Dolphin.app as an external process. The standalone adapter and its
// tests are deleted in the same commit chain.
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
};
