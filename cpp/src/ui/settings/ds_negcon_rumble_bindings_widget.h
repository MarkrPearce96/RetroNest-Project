#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

class DSNegconRumbleBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    DSNegconRumbleBindingsWidget(SdlInputManager* inputManager,
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
