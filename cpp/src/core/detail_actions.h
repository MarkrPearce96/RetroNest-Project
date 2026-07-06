#pragma once

#include <QVariantList>

#include "manifest.h"

/**
 * Ordered row model for EmulatorDetailPage's ACTIONS column (packet 7
 * stage 3 — replaces the QML's per-emulator branching and focus-index
 * arithmetic). Each row is a QVariantMap:
 *
 *   { "action": settings|controller|hotkeys|patches|reinstall|reset|uninstall,
 *     "label":  <button text>,
 *     "controllerType": <controllerTypes() id for action=="controller";
 *                        "" = the adapter's default mapping page> }
 *
 * Row order: settings → controller page(s) → hotkeys (when the emulator
 * has hotkey defs) → patches (when the manifest declares has_patches) →
 * reinstall → reset → uninstall. Controller pages come from the
 * manifest's detail_page.controller_pages; an empty list means one
 * default "Controller Mapping" row.
 *
 * Not-installed → empty list (the page shows GET STARTED instead). The
 * BIOS button is NOT part of this model — it renders in the BIOS section
 * and keeps its historical focus slot 0 via the page's actionOffset.
 */
QVariantList detailActionRows(const EmulatorManifest& m, bool installed, bool hasHotkeys);
