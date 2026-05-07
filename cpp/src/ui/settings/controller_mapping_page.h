#pragma once

#include <QDialog>
#include <QString>

class QShortcut;
class SdlInputManager;
class AppController;
class ControllerBindingsView;
struct BindingDef;

/**
 * ControllerMappingPage — host dialog for the schema-driven
 * controller mapping view. Provides the outer frame (sizing,
 * ESC-to-close, top-chrome title) and wires keyboard / gamepad
 * face-button shortcuts to the embedded ControllerBindingsView's
 * focused-binding + AppController flows:
 *
 *   A / Enter      → rebind focused binding
 *   B / Esc        → clear focused binding (or close if none focused)
 *   Y / M          → open Auto-Map menu (Keyboard + connected SDL devices)
 *   X              → close
 */
class ControllerMappingPage : public QDialog {
    Q_OBJECT
public:
    ControllerMappingPage(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          const QString& controllerTypeId,
                          QWidget* parent = nullptr);

private:
    void onAutoMapRequested();
    void onRebindRequested(const BindingDef& b);
    void onClearRequested(const BindingDef& b);

    SdlInputManager*         m_inputManager;
    AppController*           m_appController;
    QString                  m_emuId;
    QString                  m_controllerTypeId;
    ControllerBindingsView*  m_view = nullptr;
    QString                  m_capturingKey;   // INI key currently capturing
    QShortcut*               m_autoMapShortcut = nullptr;
    QShortcut*               m_closeShortcut   = nullptr;
};
