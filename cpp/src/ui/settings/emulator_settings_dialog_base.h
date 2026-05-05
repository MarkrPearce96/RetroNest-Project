#pragma once
#include <QDialog>
#include <QStack>
#include "core/setting_def.h"

class AppController;
class QStackedWidget;
class SettingsDescriptionBar;

// Shared scaffolding for the per-emulator settings dialogs (PCSX2 / DuckStation
// / PPSSPP). Subclasses build their own category hub + per-category pages;
// the base owns the QStackedWidget, history-stack navigation, focused-setting
// description bar, controller-friendly Esc/Back/Tab handling, and the
// AppController settings-session lifecycle.
class EmulatorSettingsDialogBase : public QDialog {
    Q_OBJECT
public:
    EmulatorSettingsDialogBase(AppController* app,
                               const QString& emuId,
                               QWidget* parent = nullptr);
    ~EmulatorSettingsDialogBase() override;

    AppController* appController() const { return m_app; }
    QString emuId() const { return m_emuId; }

    // Push/pop pages on the stack. hasSubTabs unsuppresses Tab/Backtab so the
    // L1/R1 "switch tab" hint works on graphics-style sub-tab pages.
    void pushPage(QWidget* page, bool hasSubTabs = false);
    void popPage();

public slots:
    void setFocusedSetting(const SettingDef& def);
    void clearFocusedSetting();

protected:
    // Subclass calls this from its ctor with its window chrome.
    // expandedSize: optional. When non-empty, push resizes to that size and
    // pop back to the hub resizes back to the dialog's initial minimum. Used
    // by PCSX2 + PPSSPP. DuckStation passes {} and never resizes.
    void setupChrome(const QString& title,
                     const QSize& minSize,
                     const QColor& windowBg,
                     const QSize& expandedSize = {});

    // Subclass installs its hub (any QWidget) here. Wires currentChanged to
    // the description bar and applies initial hints.
    void setHub(QWidget* hub);

    void applyHintsForCurrentPage();
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

    AppController* m_app = nullptr;
    QString m_emuId;
    QStackedWidget* m_stack = nullptr;
    SettingsDescriptionBar* m_descBar = nullptr;
    QWidget* m_hub = nullptr;
    QStack<int> m_history;
    bool m_currentPageHasSubTabs = false;
    QSize m_compactSize;
    QSize m_expandedSize;
};
