#pragma once

#include <QDialog>
#include <QComboBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QString>

class SdlInputManager;
class AppController;
class ControllerSettingsWidget;

/**
 * ControllerMappingPage — full controller settings dialog matching PCSX2 native.
 *
 * Layout: left sidebar (port list) | right content (toolbar + stacked bindings/settings)
 * Bottom: profile management bar.
 */
class ControllerMappingPage : public QDialog {
    Q_OBJECT

public:
    ControllerMappingPage(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          QWidget* parent = nullptr);

private slots:
    void onPortChanged(int row);
    void onTypeChanged(int index);
    void onBindingsClicked();
    void onSettingsClicked();
    void onAutoMap();
    void onClearMapping();
    void onRestoreDefaults();
    void onNewProfile();
    void onApplyProfile();
    void onRenameProfile();
    void onDeleteProfile();

private:
    void buildUI();
    void loadPort(int port);
    void switchTab(int tab); // 0 = bindings, 1 = settings
    QWidget* createBindingsWidget(const QString& type);
    void updateSidebar();
    void refreshProfiles();

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;
    int m_currentPort = 1;
    int m_currentTab = 0;
    QString m_currentType;

    // Sidebar
    QListWidget* m_portList = nullptr;

    // Toolbar
    QComboBox* m_typeCombo = nullptr;
    QToolButton* m_bindingsBtn = nullptr;
    QToolButton* m_settingsBtn = nullptr;
    QToolButton* m_autoMapBtn = nullptr;
    QToolButton* m_clearMapBtn = nullptr;

    // Content
    QStackedWidget* m_contentStack = nullptr;
    QWidget* m_bindingsWidget = nullptr;
    ControllerSettingsWidget* m_settingsWidget = nullptr;

    // Profile bar
    QComboBox* m_profileCombo = nullptr;
    QPushButton* m_applyProfileBtn = nullptr;
    QPushButton* m_renameProfileBtn = nullptr;
    QPushButton* m_deleteProfileBtn = nullptr;
};
