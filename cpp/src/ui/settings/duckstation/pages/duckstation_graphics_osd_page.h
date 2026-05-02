#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class DuckStationSettingsDialog;
class DuckStationOsdPreview;
class Pcsx2ComboRow;
class Pcsx2SliderRow;
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
    Pcsx2SliderRow*            m_scaleSlider      = nullptr;
    Pcsx2SliderRow*            m_marginSlider     = nullptr;
    Pcsx2ComboRow*             m_locationCombo    = nullptr;
};
