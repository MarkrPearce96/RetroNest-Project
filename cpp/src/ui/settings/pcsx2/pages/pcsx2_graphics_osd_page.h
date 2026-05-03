#pragma once
#include <QWidget>
#include <QVector>
#include <QList>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class SettingsCard;
class Pcsx2OsdPreview;
class SettingsComboRow;
class SettingsToggleRow;
class SettingsSliderRow;
class QHBoxLayout;
class QVBoxLayout;

class Pcsx2GraphicsOsdPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsOsdPage(Pcsx2SettingsDialog* dialog);
    ~Pcsx2GraphicsOsdPage() override;

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

    Pcsx2OsdPreview* m_preview = nullptr;

    SettingsSliderRow* m_scaleSlider = nullptr;
    SettingsComboRow*  m_messagesPosCombo = nullptr;
    SettingsComboRow*  m_perfPosCombo = nullptr;
};
