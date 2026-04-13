#pragma once
#include <QWidget>
#include <QVector>
#include <QHash>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;
class Pcsx2ToggleRow;

class Pcsx2GraphicsPostProcessingPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsPostProcessingPage(Pcsx2SettingsDialog* dialog);
    ~Pcsx2GraphicsPostProcessingPage() override;

signals:
    void settingFocused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;
    bool masterToggleState(const QString& masterKey) const;
    void refreshDependencies();
    QList<QWidget*> collectFocusables() const;
    QWidget* findNextFocusSpatial(QWidget* current, int key) const;

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
    QHash<QString, Pcsx2ToggleRow*> m_masterToggles;
};
