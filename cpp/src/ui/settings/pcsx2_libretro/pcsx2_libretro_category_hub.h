#pragma once
#include "ui/settings/emulator_category_hub_base.h"

// SP7b: category hub for the pcsx2-libretro per-emulator settings dialog.
// Mirrors MgbaCategoryHub — only the libretro option rows declared in
// Pcsx2LibretroAdapter::settingsSchema() are reachable. Currently a
// single category ("Recommended") with three rows; more categories
// can be added later if SP7b's scope expands.
class Pcsx2LibretroCategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit Pcsx2LibretroCategoryHub(QWidget* parent = nullptr);

private:
    int countSettings(const QString& category) const;
};
