#pragma once

#include <QWidget>
#include <QMap>
#include <QPushButton>
#include <QLabel>
#include <QString>

class SdlInputManager;
class AppController;
class BindBtn;

/**
 * BindingsWidgetBase — shared logic for all controller binding widgets.
 * Subclasses only need to create widgets in the constructor and implement relayout().
 */
class BindingsWidgetBase : public QWidget {
    Q_OBJECT
public:
    BindingsWidgetBase(SdlInputManager* inputManager,
                       AppController* appController,
                       const QString& emuId,
                       int port,
                       QWidget* parent = nullptr);

    void loadBindings();

protected:
    void resizeEvent(QResizeEvent* event) override;

    /** Subclasses implement this to position all widgets. */
    virtual void relayout() = 0;

    /** Register a BindBtn with a label — sets up click, right-click, style. */
    void setupBtn(BindBtn* b, const QString& label);

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;
    int m_port;
    QLabel* m_imgLabel = nullptr;
    QMap<QString, QPushButton*> m_bindingButtons;

private:
    void startCapture(const QString& label);
    void onBindingCaptured(int deviceIndex, const QString& element, bool isAxis, bool positive);
    void onKeyboardCaptured(const QString& keyString);
    void finishCapture(const QString& formatted);

    QString m_capturingLabel;
};
