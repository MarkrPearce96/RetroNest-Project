#pragma once
#include <QWidget>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class Pcsx2GraphicsSubTabBar;
class QStackedWidget;

class Pcsx2GraphicsPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsPage(Pcsx2SettingsDialog* dialog);

signals:
    void settingFocused(SettingDef def);

private slots:
    void onSubTabActivated(int index);

private:
    Pcsx2SettingsDialog* m_dialog;
    Pcsx2GraphicsSubTabBar* m_tabBar = nullptr;
    QStackedWidget* m_stack = nullptr;
};
