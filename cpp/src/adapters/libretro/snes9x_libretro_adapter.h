#pragma once
#include "libretro_adapter.h"

// Snes9x — stock upstream SNES libretro core (software render, universal CI
// release). Mirrors the mGBA adapter pattern: a self-contained core with no
// BIOS and a RetroPad-native controller. Settings render from the core's
// declared options via the base settingsSchema; curation overlays are a
// follow-up once the core is installed and its option table probed.
class Snes9xLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "snes9x"; }

    QVector<PathDef> pathsDefs() const override;
    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<BindingDef> controllerBindingDefsForType(const QString& type) const override;

    // Libretro adapters have no INI — settings dispatch via SettingDef::Storage.
    QString configFilePath() const override { return {}; }

    // GameSession writes "<basename>.resume" under the snes savestates dir;
    // locate it at next launch (mirrors MgbaLibretroAdapter).
    QString findResumeFile(const QString& serial) const override;
};
