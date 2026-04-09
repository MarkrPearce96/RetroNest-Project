#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

class GuitarBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    GuitarBindingsWidget(SdlInputManager* inputManager,
                         AppController* appController,
                         const QString& emuId,
                         int port,
                         QWidget* parent = nullptr);

protected:
    void relayout() override {}
};
