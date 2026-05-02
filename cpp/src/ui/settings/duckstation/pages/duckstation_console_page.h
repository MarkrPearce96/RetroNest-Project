#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class DuckStationSettingsDialog;
class Pcsx2Card;

class DuckStationConsolePage : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationConsolePage(DuckStationSettingsDialog* dialog);
    ~DuckStationConsolePage() override;

signals:
    void settingFocused(const SettingDef& def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    void refreshDependencies();
    const SettingDef* findDef(const QString& key) const;
    Pcsx2Card* findNextCardSpatial(Pcsx2Card* current, int key) const;

    DuckStationSettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
