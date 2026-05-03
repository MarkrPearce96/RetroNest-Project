#pragma once
#include "ui/settings/emulator_category_hub_base.h"

class Pcsx2CategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit Pcsx2CategoryHub(QWidget* parent = nullptr);
};
