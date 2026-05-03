#pragma once
#include <QWidget>
#include "core/setting_def.h"

class PpssppSettingsDialog;
class SettingsGraphicsSubTabBar;
class QStackedWidget;

class PpssppGraphicsPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppGraphicsPage(PpssppSettingsDialog* dialog);
    ~PpssppGraphicsPage() override;

signals:
    void settingFocused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private slots:
    void onSubTabActivated(int index);

private:
    void focusFirstSettingOnCurrentTab();

    PpssppSettingsDialog* m_dialog;
    SettingsGraphicsSubTabBar* m_tabBar = nullptr;
    QStackedWidget* m_stack = nullptr;
};
