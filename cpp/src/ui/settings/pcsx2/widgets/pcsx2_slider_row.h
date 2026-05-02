#pragma once
#include <QWidget>
#include <QEnterEvent>
#include <functional>
#include "core/setting_def.h"

class QLabel;
class QSlider;

class Pcsx2SliderRow : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2SliderRow(QWidget* parent = nullptr);
    void setLabel(const QString& text);
    void setRange(int lo, int hi);
    void setSuffix(const QString& s);
    void setValue(int v);
    int value() const;
    void setSettingDef(const SettingDef& def) { m_def = def; }
    const SettingDef& settingDef() const { return m_def; }
    bool isEditing() const { return m_editing; }
    void setEditing(bool on);
    // Optional override for the right-hand value label. Receives the raw int
    // value; returns the full display string (replaces "<n><suffix>"). Pass
    // an empty function to revert to the default "<value><suffix>" format.
    void setValueFormatter(std::function<QString(int)> fmt);

signals:
    void valueChanged(int v);
    void focused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;
    void enterEvent(QEnterEvent* e) override;

private:
    void refreshValueLabel();
    QLabel* m_label = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_value = nullptr;
    QString m_suffix;
    std::function<QString(int)> m_formatter;
    SettingDef m_def;
    bool m_editing = false;
};
