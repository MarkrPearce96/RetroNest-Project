#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class DuckStationSettingsDialog;

class DuckStationGraphicsAdvancedPage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationGraphicsAdvancedPage(DuckStationSettingsDialog* dialog);

signals:
    void settingFocused(const SettingDef& def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    void refreshDependencies();
    void resetDependentsOf(const QString& masterKey);
    const SettingDef* findDef(const QString& key) const;

    DuckStationSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
