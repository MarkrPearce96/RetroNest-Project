#pragma once
#include "ui/settings/emulator_category_hub_base.h"

// Category hub for the PCSX2 per-emulator settings dialog.
// Mirrors MgbaCategoryHub — only the libretro option rows declared in
// Pcsx2LibretroAdapter::settingsSchema() are reachable.
class Pcsx2CategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    explicit Pcsx2CategoryHub(QWidget* parent = nullptr);

private:
    int countSettings(const QString& category) const;
};
