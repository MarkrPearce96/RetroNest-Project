#pragma once
#include "ui/settings/emulator_category_hub_base.h"

class MgbaCategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit MgbaCategoryHub(QWidget* parent = nullptr);

private:
    int countSettings(const QString& category) const;
};
