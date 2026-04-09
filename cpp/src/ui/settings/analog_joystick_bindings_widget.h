#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

/**
 * AnalogJoystickBindingsWidget — DuckStation AnalogJoystick visual binding layout.
 * Like AnalogController but "Mode" instead of "Analog", and no motor buttons.
 */
class AnalogJoystickBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    AnalogJoystickBindingsWidget(SdlInputManager* inputManager,
                                 AppController* appController,
                                 const QString& emuId,
                                 int port,
                                 QWidget* parent = nullptr);

protected:
    void relayout() override;

private:
    QWidget* m_dpadBox = nullptr;
    QWidget* m_lAnalogBox = nullptr;
    QWidget* m_faceBox = nullptr;
    QWidget* m_rAnalogBox = nullptr;
};
