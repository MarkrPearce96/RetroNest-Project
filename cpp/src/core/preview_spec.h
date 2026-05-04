#pragma once

#include <QHash>
#include <QString>

/**
 * PreviewSpec — describes the live preview widget for a category/sub-tab.
 *
 * Returned by EmulatorAdapter::previewSpec(category, subcategory).
 * When previewType is empty, no preview is shown — GenericSettingsPage
 * renders the standard stacked card layout.
 *
 * keyToProperty maps a SettingDef::key (the bare INI key, e.g.
 * "AspectRatio") to a Q_PROPERTY name on the named preview widget
 * (e.g. "aspectMode"). When a setting widget for a mapped key changes,
 * GenericSettingsPage updates the preview via QObject::setProperty().
 */
struct PreviewSpec {
    QString previewType;                     // "aspect" | "osd" | "" (none)
    QHash<QString, QString> keyToProperty;   // SettingDef::key → Q_PROPERTY name
};
