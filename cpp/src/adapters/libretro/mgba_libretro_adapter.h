#pragma once
#include "libretro_adapter.h"

class MgbaLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "mgba"; }

    // No internal-resolution scaling (GBA), so no Resolution tab entry. Aspect
    // ratio is a RetroNest frontend setting (no aspect core option), surfaced
    // on the quick Aspect Ratio tab via aspect_mode with a curated shortlist.
    QString aspectRatioFrontendKey() const override { return "aspect_mode"; }
    QVector<QPair<QString, QString>> aspectRatioOptionShortlist() const override {
        return {{"native", "Auto"}, {"4_3", "4:3"}, {"16_9", "16:9"}};
    }

    QVector<BiosDef> biosFiles() const override;
    QVector<PathDef> pathsDefs() const override;
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
    void prepareCoreEnvironment() const override;
};
