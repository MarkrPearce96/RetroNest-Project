#include "detail_actions.h"

static QVariantMap row(const char* action, const QString& label,
                       const QString& controllerType = QString()) {
    return { {"action", action}, {"label", label}, {"controllerType", controllerType} };
}

QVariantList detailActionRows(const EmulatorManifest& m, bool installed, bool hasHotkeys) {
    if (!installed)
        return {};
    QVariantList rows;
    rows << row("settings", "Emulator Settings");
    if (m.controller_pages.isEmpty()) {
        rows << row("controller", "Controller Mapping");
    } else {
        for (const auto& page : m.controller_pages)
            rows << row("controller", page.label, page.type);
    }
    if (hasHotkeys)
        rows << row("hotkeys", "Hotkeys");
    if (m.has_patches)
        rows << row("patches", "Refresh " + m.name + " Patches");
    rows << row("reinstall", "Reinstall / Update");
    rows << row("reset", "Reset Configuration");
    rows << row("uninstall", "Uninstall");
    return rows;
}
