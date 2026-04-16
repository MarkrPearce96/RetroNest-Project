#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class DuckStationSettingsDialog;
class DuckStationOsdPreview;

class DuckStationGraphicsOsdPage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationGraphicsOsdPage(DuckStationSettingsDialog* dialog);

signals:
    void settingFocused(const SettingDef& def);

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;

    DuckStationSettingsDialog* m_dialog;
    QVector<SettingDef>        m_schema;
    DuckStationOsdPreview*     m_preview = nullptr;
};
