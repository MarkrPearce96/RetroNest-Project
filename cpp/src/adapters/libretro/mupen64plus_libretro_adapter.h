#pragma once
#include "libretro_adapter.h"

// Mupen64Plus-Next — stock upstream N64 libretro core (GLideN64 GL render +
// HLE RSP + x64 recompiler). Built x86_64-only (recompiler runs under Rosetta
// with RetroNest's allow-jit entitlement, like pcsx2/dolphin), so the manifest
// declares core_arch "x86_64". No BIOS.
//
// N64 is NOT a RetroPad reference layout: the core's default descriptor maps
// RetroPad slots onto N64 buttons in a fixed non-1:1 way (A→JOYPAD_B,
// B→JOYPAD_Y, C-buttons→A/X/L/R, Z→L2, N64-R→R2, N64-L→SELECT). So each
// binding carries an explicit defaultValue that reproduces the standard
// RetroArch feel (bottom face = N64 A, left face = N64 B, C-cluster on the
// right face + bumpers, Z on the left trigger). The analog stick is the host
// left stick, fixed-routed by SdlInputManager (not controls.ini-remappable).
// Settings render from the core's declared options via the base
// settingsSchema; curation overlays are a follow-up once the core is probed.
class Mupen64PlusLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "mupen64plus"; }

    QVector<PathDef> pathsDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;

    // Libretro adapters have no INI — settings dispatch via SettingDef::Storage.
    QString configFilePath() const override { return {}; }

    // GLideN64 renders via OpenGL (SET_HW_RENDER OPENGL_CORE), so the session
    // must use the GL display path (LibretroGLItem samples our IOSurface FBO),
    // NOT the base default (None → software item, which would never composite
    // the core's frames → black screen). Same as PPSSPP.
    HardwareRenderBackend hardwareRenderBackend() const override {
        return HardwareRenderBackend::GL;
    }

    // GameSession writes "<basename>.resume" under the n64 savestates dir;
    // locate it at next launch (mirrors the other libretro adapters).
    QString findResumeFile(const QString& serial) const override;

protected:
    // Upstream dylib stem is "mupen64plus_next", not the coreId "mupen64plus",
    // so the {coreId}_libretro.dylib convention doesn't hold — match the
    // manifest's core_dylib so the offline options prober finds the core.
    QString coreDylibFileName() const override {
        return QStringLiteral("mupen64plus_next_libretro.dylib");
    }
};
