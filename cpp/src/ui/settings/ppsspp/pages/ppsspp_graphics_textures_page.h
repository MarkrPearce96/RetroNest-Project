#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class PpssppSettingsDialog;

class PpssppGraphicsTexturesPage : public QWidget {
    Q_OBJECT
public:
    explicit PpssppGraphicsTexturesPage(PpssppSettingsDialog* dialog);
signals:
    void settingFocused(SettingDef def);
private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    PpssppSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
