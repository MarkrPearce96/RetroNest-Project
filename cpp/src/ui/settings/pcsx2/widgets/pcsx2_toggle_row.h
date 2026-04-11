#pragma once
#include <QWidget>
#include <QEnterEvent>
#include "core/setting_def.h"

class QLabel;
class Pcsx2Toggle;

class Pcsx2ToggleRow : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2ToggleRow(QWidget* parent = nullptr);
    void setLabel(const QString& text);
    void setChecked(bool on);
    bool isChecked() const;
    void setSettingDef(const SettingDef& def) { m_def = def; }
    const SettingDef& settingDef() const { return m_def; }

signals:
    void toggled(bool on);
    void focused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;
    void enterEvent(QEnterEvent* e) override;

private:
    QLabel* m_label = nullptr;
    Pcsx2Toggle* m_toggle = nullptr;
    SettingDef m_def;
};
