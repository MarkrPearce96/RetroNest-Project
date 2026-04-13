#pragma once
#include <QWidget>
#include <QVector>
#include "core/setting_def.h"

class Pcsx2SettingsDialog;

class Pcsx2GraphicsRenderingPage : public QWidget {
    Q_OBJECT
public:
    explicit Pcsx2GraphicsRenderingPage(Pcsx2SettingsDialog* dialog);
    ~Pcsx2GraphicsRenderingPage() override;

signals:
    void settingFocused(SettingDef def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    const SettingDef* findDef(const QString& key) const;
    QList<QWidget*> collectFocusables() const;
    QWidget* findNextFocusSpatial(QWidget* current, int key) const;

    Pcsx2SettingsDialog* m_dialog;
    QVector<SettingDef> m_schema;
};
