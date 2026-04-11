#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class AppController;
class QStackedWidget;
class Pcsx2DescriptionBar;
class Pcsx2CategoryHub;

class Pcsx2SettingsDialog : public QDialog {
    Q_OBJECT
public:
    Pcsx2SettingsDialog(AppController* app, const QString& emuId, QWidget* parent = nullptr);

    // Navigation API used by child pages
    void pushPage(QWidget* page);
    void popPage();
    AppController* appController() const { return m_app; }
    QString emuId() const { return m_emuId; }

public slots:
    void setFocusedSetting(const SettingDef& def);
    void clearFocusedSetting();

private slots:
    void onCategoryActivated(const QString& category);

private:
    AppController* m_app;
    QString m_emuId;
    QStackedWidget* m_stack = nullptr;
    Pcsx2DescriptionBar* m_descBar = nullptr;
    Pcsx2CategoryHub* m_hub = nullptr;
    QStack<int> m_history;
};
