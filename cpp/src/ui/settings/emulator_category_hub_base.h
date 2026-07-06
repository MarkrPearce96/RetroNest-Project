#pragma once
#include <QWidget>

class SettingsCard;
class QPushButton;
class QVBoxLayout;

// Shared scaffolding for the category-hub widgets shown by each
// EmulatorSettingsDialogBase subclass. The hub renders a title row, a
// content area (filled by the subclass with one or more category card
// grids), and a "Open Native Settings" button at the bottom. Subclasses
// only declare which cards exist; this class owns the chrome and the
// makeCard rendering that's identical across PCSX2/DuckStation/PPSSPP.
class EmulatorCategoryHubBase : public QWidget {
    Q_OBJECT
public:
    explicit EmulatorCategoryHubBase(QWidget* parent = nullptr);

signals:
    void categoryActivated(QString category);

protected:
    // Build root layout: title label, content area, native-settings button.
    // Subclass adds its grid(s) into contentLayout() after calling this.
    void setupChrome(const QString& title);
    QVBoxLayout* contentLayout() { return m_contentLayout; }

    // Standard category card. Wires SettingsCard::activated → categoryActivated(categoryKey).
    SettingsCard* makeCard(const QString& icon, const QString& title,
                        const QString& descriptor, int settingCount,
                        const QString& categoryKey);

    // Default: Up from native button → focus last direct-child SettingsCard.
    // Down from a SettingsCard → focus the native button.
    // Subclass overrides for non-grid layouts (e.g. DuckStation's stretchCard).
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    QVBoxLayout* m_contentLayout = nullptr;
};
