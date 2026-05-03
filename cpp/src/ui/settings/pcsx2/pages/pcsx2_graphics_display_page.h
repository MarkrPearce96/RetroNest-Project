#pragma once
#include <QWidget>
#include <QVector>
#include <QList>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class SettingsCard;
class Pcsx2AspectRatioPreview;
class SettingsComboRow;
class SettingsToggleRow;
class SettingsSliderRow;
class QSpinBox;
class QHBoxLayout;
class QVBoxLayout;

class Pcsx2GraphicsDisplayPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsDisplayPage(Pcsx2SettingsDialog* dialog);
    ~Pcsx2GraphicsDisplayPage() override;

signals:
    void settingFocused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void buildLeftCompoundCard(QHBoxLayout* topRow);
    void buildRightPreviewCard(QHBoxLayout* topRow);
    void buildBottomToggleGrid(QVBoxLayout* root);

    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    void syncPreview();

    QList<QWidget*> collectFocusables() const;
    QWidget* findNextFocusSpatial(QWidget* current, int key) const;

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef>  m_schema;

    Pcsx2AspectRatioPreview* m_preview = nullptr;
    SettingsComboRow*  m_aspectCombo = nullptr;
    SettingsSliderRow* m_stretchSlider = nullptr;
    QSpinBox* m_cropL = nullptr;
    QSpinBox* m_cropT = nullptr;
    QSpinBox* m_cropR = nullptr;
    QSpinBox* m_cropB = nullptr;
    SettingsToggleRow* m_integerScalingToggle = nullptr;
};
