#pragma once

#include "bindings_widget_base.h"

class SdlInputManager;
class AppController;

class PopnBindingsWidget : public BindingsWidgetBase {
    Q_OBJECT
public:
    // Pop'n controller comes in two flavours that differ only in their button
    // label set + ordering (the binding keys the adapter writes). PCSX2 uses
    // "Pcsx2" labels; DuckStation uses "DuckStation" labels.
    enum class Variant { Pcsx2, DuckStation };

    PopnBindingsWidget(SdlInputManager* inputManager,
                       AppController* appController,
                       const QString& emuId,
                       int port,
                       Variant variant = Variant::Pcsx2,
                       QWidget* parent = nullptr);

protected:
    void relayout() override {}
};
