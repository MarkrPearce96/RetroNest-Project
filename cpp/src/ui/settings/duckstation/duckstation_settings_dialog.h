#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class QStackedWidget;
class AppController;
class DuckStationCategoryHub;
class Pcsx2DescriptionBar;

class DuckStationSettingsDialog : public QDialog {
    Q_OBJECT
public:
    DuckStationSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

    AppController* appController() const { return m_app; }
    const QString& emuId() const { return m_emuId; }

    void pushPage(QWidget* page, bool hasSubTabs = false);
    void popPage();
    void setFocusedSetting(const SettingDef& def);
    void clearFocusedSetting();

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void onCategoryActivated(const QString& category);
    void applyHintsForCurrentPage();

    AppController* m_app;
    QString m_emuId;
    QStackedWidget* m_stack = nullptr;
    DuckStationCategoryHub* m_hub = nullptr;
    Pcsx2DescriptionBar* m_descBar = nullptr;
    QStack<int> m_history;
    bool m_currentPageHasSubTabs = false;
};
