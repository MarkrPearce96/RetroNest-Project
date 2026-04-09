#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

/**
 * PSPBindingsWidget — PPSSPP PSP Controller visual binding layout.
 * 16 bindings: D-Pad (4), Face (4), L/R (2), Start/Select (2), Analog (4).
 */
class PSPBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    PSPBindingsWidget(SdlInputManager* inputManager,
                      AppController* appController,
                      const QString& emuId,
                      int port,
                      QWidget* parent = nullptr);

protected:
    void relayout() override;

private:
    QWidget* m_dpadBox = nullptr;
    QWidget* m_faceBox = nullptr;
    QWidget* m_analogBox = nullptr;
};
