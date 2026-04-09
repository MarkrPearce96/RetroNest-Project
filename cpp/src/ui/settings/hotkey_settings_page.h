#pragma once

#include <QDialog>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

class SdlInputManager;
class AppController;

/**
 * HotkeySettingsPage — Qt Widget dialog for hotkey binding configuration.
 *
 * Layout: left sidebar (4 categories) + right content (grid of label + binding button).
 */
class HotkeySettingsPage : public QDialog {
    Q_OBJECT

public:
    struct HotkeyEntry {
        QString label;
        QString group;
        QString section;
        QString key;
        QString currentValue;
    };

    HotkeySettingsPage(SdlInputManager* inputManager,
                       AppController* appController,
                       const QString& emuId,
                       QWidget* parent = nullptr);

private:
    void buildUI();
    void loadBindings();
    void showCategory(int index);
    void startCapture(const QString& key, bool append = false);
    void stopCapture(bool save);
    bool eventFilter(QObject* obj, QEvent* event) override;
    void onBindingCaptured(int deviceIndex, const QString& element, bool isAxis, bool positive);
    void onKeyboardCaptured(const QString& keyString);
    void finishCapture(const QString& formatted);
    void onResetDefaults();

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;

    QListWidget* m_categoryList = nullptr;
    QVector<QWidget*> m_categoryPages;

    QMap<QString, QPushButton*> m_bindingButtons; // iniKey -> button
    QString m_capturingKey;
    bool m_appendMode = false;

    // Capture system
    QTimer* m_captureTimer = nullptr;
    int m_captureCountdown = 0;
    QStringList m_capturedBindings; // accumulated during capture window

    QStringList m_categories;
    QVector<HotkeyEntry> m_entries; // preserves adapter definition order
};
