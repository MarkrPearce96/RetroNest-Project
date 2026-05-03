#pragma once
#include <QWidget>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class SettingsGraphicsSubTabBar;
class QStackedWidget;

class Pcsx2GraphicsPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsPage(Pcsx2SettingsDialog* dialog);
    ~Pcsx2GraphicsPage() override;

signals:
    void settingFocused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private slots:
    void onSubTabActivated(int index);

private:
    void focusFirstSettingOnCurrentTab();

    Pcsx2SettingsDialog* m_dialog;
    SettingsGraphicsSubTabBar* m_tabBar = nullptr;
    QStackedWidget* m_stack = nullptr;
};
