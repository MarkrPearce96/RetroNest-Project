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
    QVector<SettingDef> settingsSchema() const override;
    PreviewSpec previewSpec(const QString& category, const QString& subcategory) const override;
    QString configFilePath() const override;

    QString extractSerial(const QString& romPath) const override;
    QString findResumeFile(const QString& serial) const override;
    int raConsoleId(const QString& systemId) const override;
};
