#pragma once
#include <QString>
#include <QVector>
#include <functional>
#include "core/setting_def.h"

class QWidget;
class SettingsCard;

// Helper that owns the boilerplate every settings page repeats: the three
// makeXxxCard lambdas (combo / slider / toggle) and the scroll-area
// stylesheet. The page passes its schema slice plus save / focus callbacks;
// the builder hands back configured SettingsCards wired to those callbacks.
class SettingsPageBuilder {
public:
    using SaveFn = std::function<void(const QString& section, const QString& key,
                                      const QString& value)>;
    using ReadFn = std::function<QString(const QString& section,
                                          const QString& key)>;
    using FocusFn = std::function<void(const SettingDef&)>;

    SettingsPageBuilder(QWidget* parentPage,
                        const QVector<SettingDef>& schema,
                        SaveFn save,
                        ReadFn read,
                        FocusFn focus);

    SettingsCard* makeComboCard(const QString& key);
    SettingsCard* makeSliderCard(const QString& key);
    // Multiple SettingDefs may share the same key when they pack different
    // bits of one packed-int INI value (e.g. PPSSPP's iShowStatusFlags
    // hosts FPS/Speed/Battery on bits 2/4/8). Pass the matching bitmask
    // to disambiguate; bitmask=0 is the default — a normal Bool toggle.
    SettingsCard* makeToggleCard(const QString& key, int bitmask = 0);

    // Identical across every settings page. Apply once per QScrollArea.
    static const char* kScrollAreaQss;

private:
    const SettingDef* findDef(const QString& key, int bitmask = 0) const;

    QWidget* m_parent;
    const QVector<SettingDef>& m_schema;
    SaveFn m_save;
    ReadFn m_read;
    FocusFn m_focus;
};
