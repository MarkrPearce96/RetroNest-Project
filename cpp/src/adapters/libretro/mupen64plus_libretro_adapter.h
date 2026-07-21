#pragma once
#include "libretro_adapter.h"

// Mupen64Plus-Next — N64 libretro core from the fork
// MarkrPearce96/mupen64plus-libretro-nx (GLideN64 GL render + HLE RSP +
// new_dynarec recompilers: aarch64 native on Apple Silicon, x86_64 under
// Rosetta). Ships UNIVERSAL per the standing arch policy; the manifest's
// core_arch must be "universal" once the universal CI release is live. No BIOS.
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

    // Settings curation (schema = core-declared table × these overlays).
    QVector<OptionOverlay> optionOverlays() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;

    // Quick-settings tabs read/write these core option keys through the
    // options.json pipeline. Resolution = GLideN64's native-res multiplier
    // (0 = the classic 4:3/16:9 fixed-size lists, gated in the overlay);
    // curated pill shortlist (full value set stays on the main settings page).
    QString resolutionOptionKey() const override { return "mupen64plus-EnableNativeResFactor"; }
    QString aspectRatioOptionKey() const override { return "mupen64plus-aspect"; }
    QVector<QPair<QString, QString>> resolutionOptionShortlist() const override {
        // N64 native is 320x240 — 1x..8x spans 240p to supersampled ~1920p.
        return {{"1", "1x"}, {"2", "2x"}, {"4", "4x"}, {"8", "8x"}};
    }
    QVector<QPair<QString, QString>> aspectRatioOptionShortlist() const override {
        return {{"4:3", "4:3"}, {"16:9", "16:9 Stretched"},
                {"16:9 adjusted", "16:9 Adjusted"}};
    }

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
