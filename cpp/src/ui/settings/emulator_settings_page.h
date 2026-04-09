#pragma once

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QString>
#include <QVector>
#include "core/setting_def.h"

class AppController;

/**
 * EmulatorSettingsPage — Qt Widget dialog for emulator configuration.
 *
 * Left sidebar with categories, sub-tabs, and grouped settings with
 * combo boxes, spin boxes, checkboxes, sliders, and text fields.
 * Matches the dark theme of the rest of the app.
 */
class EmulatorSettingsPage : public QDialog {
    Q_OBJECT

public:
    EmulatorSettingsPage(AppController* appController,
                         const QString& emuId,
                         QWidget* parent = nullptr);

private:
    void buildUI();
    QWidget* buildCategoryPage(const QString& category);
    QWidget* buildSubcategoryContent(const QString& category, const QString& subcategory);
    QWidget* buildGroupBox(const QString& groupName, const QVector<SettingDef>& settings,
                           bool singleColumnBools = false);

    void onCategoryChanged(int row);

    AppController* m_appController;
    QString m_emuId;
    QVector<SettingDef> m_schema;
    QStringList m_categories;

    QListWidget* m_categoryList = nullptr;
    QStackedWidget* m_contentStack = nullptr;
};
