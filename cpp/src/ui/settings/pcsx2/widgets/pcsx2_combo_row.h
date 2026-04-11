#pragma once
#include <QWidget>
#include <QVector>
#include <QPair>
#include <QEnterEvent>
#include "core/setting_def.h"

class QLabel;
class QComboBox;

class Pcsx2ComboRow : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2ComboRow(QWidget* parent = nullptr);
    void setLabel(const QString& text);
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
};
