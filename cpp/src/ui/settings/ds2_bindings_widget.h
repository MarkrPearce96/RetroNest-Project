#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

/**
 * DS2BindingsWidget — DualShock 2 visual binding layout.
 * Positions all 28 binding buttons in the PCSX2-native diamond layout.
 */
class DS2BindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    DS2BindingsWidget(SdlInputManager* inputManager,
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
