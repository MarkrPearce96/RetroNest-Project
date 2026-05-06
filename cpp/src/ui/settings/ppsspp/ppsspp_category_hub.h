#pragma once
#include "ui/settings/emulator_category_hub_base.h"

class PpssppCategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit PpssppCategoryHub(QWidget* parent = nullptr);

private:
    int countSettings(const QString& category) const;
};
