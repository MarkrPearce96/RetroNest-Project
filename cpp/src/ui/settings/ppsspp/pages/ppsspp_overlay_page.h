#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppOverlayPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppOverlayPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveBitmaskBit(const SettingDef& def, bool checked);
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findByLabel(const QString& label) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
