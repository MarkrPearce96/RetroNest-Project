#pragma once
#include <QWidget>
#include <QVector>
#include <QPair>
#include <QEnterEvent>
#include "core/setting_def.h"

class QLabel;
class QComboBox;

class SettingsComboRow : public QWidget {
    Q_OBJECT
public:
    explicit SettingsComboRow(QWidget* parent = nullptr, bool stacked = false);
    void setLabel(const QString& text);
    // Hide the label entirely when packing multiple compact combos into one
    // row (e.g. Screen Position pair: alignment + rotation share a single
    // label slot to the left).
    void setLabelVisible(bool visible);
    void setOptions(const QVector<QPair<QString, QString>>& opts);
    void setValue(const QString& iniValue);
    QString value() const;
    void setSettingDef(const SettingDef& def) { m_def = def; }
    const SettingDef& settingDef() const { return m_def; }

signals:
    void valueChanged(QString iniValue);
    void focused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;
    void enterEvent(QEnterEvent* e) override;

private:
    QLabel* m_label = nullptr;
    QComboBox* m_combo = nullptr;
    SettingDef m_def;
    bool m_justClosed = false;
};
