#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

/**
 * AnalogBindingsWidget — DuckStation AnalogController visual binding layout.
 * Like DS2 but without Pressure Modifier — bottom center row is L3, Analog, R3.
 */
class AnalogBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    AnalogBindingsWidget(SdlInputManager* inputManager,
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
