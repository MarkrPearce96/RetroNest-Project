#pragma once
#include "ui/settings/emulator_category_hub_base.h"

class SettingsCard;

class DuckStationCategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit DuckStationCategoryHub(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    int countSettings(const QString& category) const;

    SettingsCard* m_stretchCard = nullptr;
};
