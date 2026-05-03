#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class DuckStationSettingsDialog;
class DuckStationOsdPreview;
class SettingsComboRow;
class SettingsSliderRow;
class QHBoxLayout;
class QVBoxLayout;

class DuckStationGraphicsOsdPage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationGraphicsOsdPage(DuckStationSettingsDialog* dialog);
    ~DuckStationGraphicsOsdPage() override;

signals:
    void settingFocused(const SettingDef& def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    void buildLeftCompoundCard(QHBoxLayout* topRow);
    void buildRightPreviewCard(QHBoxLayout* topRow);
    void buildBottomToggleGrid(QVBoxLayout* root);
    void syncPreview();

    QList<QWidget*> collectFocusables() const;
    QWidget* findNextFocusSpatial(QWidget* current, int key) const;

    DuckStationSettingsDialog* m_dialog;
    QVector<SettingDef>        m_schema;
    DuckStationOsdPreview*     m_preview          = nullptr;
    SettingsSliderRow*            m_scaleSlider      = nullptr;
    SettingsSliderRow*            m_marginSlider     = nullptr;
    SettingsComboRow*             m_locationCombo    = nullptr;
};
