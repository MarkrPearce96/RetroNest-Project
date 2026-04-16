#pragma once
#include <QWidget>
#include "core/setting_def.h"

class DuckStationSettingsDialog;
class Pcsx2GraphicsSubTabBar;
class QStackedWidget;

class DuckStationGraphicsPage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationGraphicsPage(DuckStationSettingsDialog* dialog);
    ~DuckStationGraphicsPage();

    void replaceSubPage(int index, QWidget* page);

signals:
    void settingFocused(const SettingDef& def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void onSubTabActivated(int index);
    void focusFirstSettingOnCurrentTab();

    DuckStationSettingsDialog* m_dialog;
    Pcsx2GraphicsSubTabBar* m_tabBar = nullptr;
    QStackedWidget* m_stack = nullptr;
};
