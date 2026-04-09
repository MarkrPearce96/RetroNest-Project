#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

/**
 * DigitalBindingsWidget — DuckStation DigitalController visual binding layout.
 * 14 buttons: D-Pad, Face, L1/R1, L2/R2, Select, Start (no sticks, no motors).
 */
class DigitalBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    DigitalBindingsWidget(SdlInputManager* inputManager,
                          AppController* appController,
                          const QString& emuId,
                          int port,
                          QWidget* parent = nullptr);

protected:
    void relayout() override;

private:
    QWidget* m_dpadBox = nullptr;
    QWidget* m_faceBox = nullptr;
};
