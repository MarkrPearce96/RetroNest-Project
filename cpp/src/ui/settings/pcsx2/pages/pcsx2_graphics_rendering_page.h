#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;

class Pcsx2GraphicsRenderingPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsRenderingPage(Pcsx2SettingsDialog* dialog);

signals:
    void settingFocused(SettingDef def);

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
