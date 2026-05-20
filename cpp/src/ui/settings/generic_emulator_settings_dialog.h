#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"
#include "ui/settings/emulator_category_hub_base.h"
#include <QSet>

class EmulatorAdapter;

// Single category hub driven by EmulatorAdapter::settingsHubCards().
// Replaces the per-emulator subclasses (DuckStation/Dolphin/Pcsx2/Mgba/
// PpssppCategoryHub) — the grid layout now lives as data on the adapter.
class GenericCategoryHub : public EmulatorCategoryHubBase {
    Q_OBJECT
public:
    GenericCategoryHub(const QString& title,
                       const EmulatorAdapter* adapter,
                       QWidget* parent = nullptr);
};

// Single per-emulator settings dialog driven by adapter metadata.
// Replaces the 5 per-emulator dialog subclasses. AppController instantiates
// one of these for any emuId whose adapter exposes settingsHubCards().
class GenericEmulatorSettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    GenericEmulatorSettingsDialog(AppController* app,
                                  const QString& emuId,
                                  const QString& displayName,
                                  EmulatorAdapter* adapter,
                                  QWidget* parent = nullptr);

private:
    void onCategoryActivated(const QString& category);

    EmulatorAdapter* m_adapter = nullptr;
    QSet<QString> m_subTabCategories;
};
