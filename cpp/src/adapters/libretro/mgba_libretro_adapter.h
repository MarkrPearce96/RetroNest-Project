#pragma once
#include "libretro_adapter.h"

class MgbaLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "mgba"; }

    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
    ResolutionOptions resolutionOptions() const override;
    AspectRatioOptions aspectRatioOptions() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;
    QVector<HotkeyDef> hotkeyBindingDefs() const override;
    // Packet 7 Stage 2: schema rendered from the core's declared options
    // (LibretroAdapter::settingsSchema base merge) — this adapter supplies
    // only the curation overlay + the frontend-owned rows.
    QVector<OptionOverlay> optionOverlays() const override;
    QVector<SettingDef> extraSettings() const override;
    QVector<SettingsHubCard> settingsHubCards() const override;
    PreviewSpec previewSpec(const QString& category, const QString& subcategory) const override;
    QString configFilePath() const override;

    QString extractSerial(const QString& romPath) const override;
    QString findResumeFile(const QString& serial) const override;

protected:
    QVector<QPair<QString, QString>> frontendSettingDefaults() const override;
};
