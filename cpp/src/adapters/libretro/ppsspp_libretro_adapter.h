#pragma once
#include "libretro_adapter.h"

// Minimal skeleton — phase-1 libretro replacement for the standalone
// PPSSPPAdapter. Mirrors the Pcsx2LibretroAdapter pattern: just enough
// surface for the registry + a first end-to-end boot. Settings schema,
// hotkeys, controller binding defs, and richer path overrides land in
// follow-up phases.
class PpssppLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "ppsspp"; }
    HardwareRenderBackend hardwareRenderBackend() const override {
        return HardwareRenderBackend::GL;
    }

    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<PathDef> pathsDefs() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    // Packet 7 Stage 2: schema renders from the core's declared options
    // (LibretroAdapter::settingsSchema base merge) — this adapter supplies
    // routing/curation only, plus the FrontendSetting aspect rows.
    QVector<OptionOverlay> optionOverlays() const override;
    QVector<SettingDef> extraSettings() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    QVector<QPair<QString, QString>> frontendSettingDefaults() const override;
    PreviewSpec previewSpec(const QString& category,
                            const QString& subcategory) const override;

    QString extractSerial(const QString& romPath) const override;

    // The core resolves its bundled assets (fonts, flash0, compat.ini, …)
    // at <system_dir>/PPSSPP/. Returns the shipped resources dir when it
    // exists on disk; empty otherwise (→ Paths::biosDir() fallback). See
    // the .cpp for the two supported on-disk layouts.
    QString systemDirOverride() const override;

    // GameSession::terminate writes "{serial}.resume" under
    // emulators/ppsspp/psp/savestates/. Mirror that path on the load
    // side so Resume picks up Save & Quit state. Mirrors
    // Pcsx2LibretroAdapter::findResumeFile (ppsspp / psp instead of
    // pcsx2 / ps2).
    QString findResumeFile(const QString& serial) const override;
};
