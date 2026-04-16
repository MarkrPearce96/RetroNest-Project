#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class QHBoxLayout;
class QVBoxLayout;
class DuckStationSettingsDialog;
class DuckStationAspectRatioPreview;
class Pcsx2ComboRow;
class Pcsx2SliderRow;
class Pcsx2ToggleRow;

class DuckStationGraphicsRenderingPage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationGraphicsRenderingPage(DuckStationSettingsDialog* dialog);
    ~DuckStationGraphicsRenderingPage();

signals:
    void settingFocused(const SettingDef& def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void buildLeftCompoundCard(QHBoxLayout* topRow);
    void buildRightPreviewCard(QHBoxLayout* topRow);
    void buildBottomToggleGrid(QVBoxLayout* root);
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    void syncPreview();
    const SettingDef* findDef(const QString& key) const;

    QList<QWidget*> collectFocusables() const;
    QWidget* findNextFocusSpatial(QWidget* current, int key) const;

    DuckStationSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
    DuckStationAspectRatioPreview* m_preview = nullptr;
    Pcsx2ComboRow* m_aspectCombo = nullptr;
};
