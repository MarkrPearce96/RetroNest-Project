#pragma once
#include <QWidget>
#include "core/setting_def.h"
#include "core/preview_spec.h"

class EmulatorSettingsDialogBase;
class EmulatorAdapter;
class QStackedWidget;
class SettingsGraphicsSubTabBar;
class SettingsCard;

/**
 * Schema-driven settings page used by every emulator's in-app dialog.
 *
 * Constructor inputs:
 *   - dlg: parent dialog (back-button target, save-callback owner via
 *     dlg->appController() and dlg->emuId()).
 *   - categorySchema: schema entries pre-filtered to one category. May
 *     contain multiple subcategories — each becomes a sub-tab.
 *   - adapter: queried for previewSpec(category, subcategory) per active
 *     sub-tab. Owned by caller; pointer must outlive this page.
 *
 * Behaviour: groups by subcategory then SettingDef::group, dispatches on
 * SettingDef::type to make Combo/Toggle/Slider cards (via SettingsPageBuilder),
 * loads values from AppController, saves on widget change (or via
 * SettingDef::saveTransform when set), live-binds preview properties,
 * handles dependsOn / bitmask / spatial nav.
 *
 * Tasks 10-15 fill in the per-section rendering + load/save/binding logic.
 * This skeleton sets up the chrome (back button, scroll area, sub-tab bar
 * + stack when multiple subcategories) and stubs the remaining methods.
 */
class GenericSettingsPage : public QWidget {
    Q_OBJECT
public:
    GenericSettingsPage(EmulatorSettingsDialogBase* dlg,
                        QVector<SettingDef> categorySchema,
                        EmulatorAdapter* adapter,
                        QWidget* parent = nullptr);
    ~GenericSettingsPage() override;

signals:
    void settingFocused(const SettingDef& def);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void buildUi();
    void buildSubcategory(const QString& subcategory);
    void loadValues();
    void saveValue(const QString& section, const QString& key, const QString& value);
    // Two-tier so cross-card masters work: refreshDependencies() applies
    // local changes AND broadcasts to sibling GenericSettingsPage instances
    // on the same dialog. refreshDependenciesLocal() does the actual row
    // re-evaluation — gathering masters via m_dlg->findChildren so other
    // pages' Combos / Toggles are visible. Without the broadcast,
    // changing pcsx2_renderer in Recommended wouldn't grey out dependent
    // rows in Graphics until the user happened to tweak something on
    // Graphics directly (see [[cross-category-dependson-limitation]] for
    // the bug this addresses).
    void refreshDependencies();
    void refreshDependenciesLocal();
    SettingsCard* findNextCardSpatial(SettingsCard* current, int key) const;
    QWidget* mountPreviewWidget(const QString& previewType, QWidget* parent);
    void wirePreviewBinding(const PreviewSpec& spec, QWidget* preview);

    // Drop focus onto the first focusable SettingsCard in the visible
    // sub-stack page (multi-subcategory) or in the page (single) — called
    // after sub-tab activation so the spatial-nav handler always has a
    // focused starting point.
    void focusFirstSettingOnCurrentSubTab();

    EmulatorSettingsDialogBase* m_dlg = nullptr;
    EmulatorAdapter* m_adapter = nullptr;
    QString m_category;                       // common to every entry in m_schema
    QVector<SettingDef> m_schema;             // pre-filtered to one category
    QStringList m_subcategories;              // ordered, deduplicated
    SettingsGraphicsSubTabBar* m_subTabBar = nullptr;
    QStackedWidget* m_subStack = nullptr;     // one page per subcategory
    QWidget* m_currentPreview = nullptr;      // active preview widget, if any
};
