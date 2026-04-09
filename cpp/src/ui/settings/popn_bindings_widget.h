#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

class PopnBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    PopnBindingsWidget(SdlInputManager* inputManager,
                       AppController* appController,
                       const QString& emuId,
                       int port,
                       QWidget* parent = nullptr);

protected:
    void relayout() override {}
};
