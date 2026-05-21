#pragma once
#include "libretro_adapter.h"

// Minimal skeleton — phase-1 libretro replacement for the standalone
// PPSSPPAdapter. Mirrors the Pcsx2LibretroAdapter pattern: just enough
// surface for the registry + a first end-to-end boot. Settings schema,
// hotkeys, controller binding defs, and richer path overrides land in
// follow-up phases.
class PpssppLibretroAdapter : public LibretroAdapter {
    Q_OBJECT
public:
    QString coreId() const override { return "ppsspp"; }
    HardwareRenderBackend hardwareRenderBackend() const override {
        return HardwareRenderBackend::GL;
    }

    // RC_CONSOLE_PSP = 41. Without this, rc_libretro_memory_init fails
    // and achievements never trigger — see Pcsx2LibretroAdapter::raConsoleId.
    int raConsoleId(const QString& systemId) const override {
        return (systemId == "psp") ? 41 : 0;
    }

    QVector<ControllerTypeDef> controllerTypes() const override;
    QVector<PathDef> pathsDefs() const override;

    QString extractSerial(const QString& romPath) const override;
};
