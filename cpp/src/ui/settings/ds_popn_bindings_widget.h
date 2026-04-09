#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

class DSPopnBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    DSPopnBindingsWidget(SdlInputManager* inputManager,
                         AppController* appController,
                         const QString& emuId,
                         int port,
                         QWidget* parent = nullptr);

protected:
    void relayout() override {}
};
