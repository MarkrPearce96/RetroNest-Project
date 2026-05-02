#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class DuckStationSettingsDialog;
class QLabel;

class DuckStationAudioPage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationAudioPage(DuckStationSettingsDialog* dialog);

signals:
    void settingFocused(const SettingDef& def);

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    void refreshLatencyLabel();
    const SettingDef* findDef(const QString& key) const;

    DuckStationSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
    QLabel*             m_latencyLabel = nullptr;
};
