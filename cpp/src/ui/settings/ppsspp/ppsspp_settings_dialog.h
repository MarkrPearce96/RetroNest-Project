#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class AppController;
class QStackedWidget;
class Pcsx2DescriptionBar;
class PpssppCategoryHub;

class PpssppSettingsDialog : public QDialog {
    Q_OBJECT
public:
    PpssppSettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

    void pushPage(QWidget* page, bool hasSubTabs = false);
    void popPage();
    AppController* appController() const { return m_app; }
    QString emuId() const { return m_emuId; }

public slots:
    void setFocusedSetting(const SettingDef& def);
    void clearFocusedSetting();

protected:
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void onCategoryActivated(const QString& category);

private:
    void applyHintsForCurrentPage();

    AppController* m_app;
    QString m_emuId;
    QStackedWidget* m_stack = nullptr;
    Pcsx2DescriptionBar* m_descBar = nullptr;
    PpssppCategoryHub* m_hub = nullptr;
    QStack<int> m_history;
    bool m_currentPageHasSubTabs = false;
};
