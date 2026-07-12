#include "hotkey_settings_dialog.h"
#include "core/libretro/libretro_hotkey_controller.h"
#include "core/libretro/libretro_hotkey_defs.h"
#include "ui/settings/generic_hotkey_page.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_description_bar.h"
#include "ui/app_controller.h"

#include <QKeyEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QTimer>

HotkeySettingsDialog::~HotkeySettingsDialog() {
    if (m_suppressedHotkeys)
        m_suppressedHotkeys->releaseSuppression();
}

HotkeySettingsDialog::HotkeySettingsDialog(SdlInputManager* inputManager,
                                            AppController* appController,
                                            const QString& emuId,
                                            QWidget* parent)
    : EmulatorSettingsDialogBase(appController, emuId, parent),
      m_inputManager(inputManager) {
    // Hold a suppression refcount on the libretro hotkey engine for this
    // dialog's whole lifetime: while the user captures a binding here,
    // the matcher must never fire the very action being bound. This is
    // structural (review R3) — it no longer relies on the dialog only
    // ever being opened via the settings overlay keeping the QML
    // suppression Binding asserted.
    if (appController && appController->libretroHotkeys()) {
        m_suppressedHotkeys = appController->libretroHotkeys();
        m_suppressedHotkeys->acquireSuppression();
    }

    setupChrome(QStringLiteral("Hotkey Settings"),
                QSize(900, 720),
                SettingsDialogTheme::windowBg());

    // Two-column layout (Keyboard | Controller) for the libretro hotkeys
    // sentinel emuId — per-emulator standalone dialogs keep the single
    // combined column they've always used.
    const bool dualColumn = (emuId == libretro_hotkeys::kSentinelEmuId);
    m_page = new GenericHotkeyPage(inputManager, appController, emuId,
                                    this, dualColumn);
    connect(m_page, &GenericHotkeyPage::bindingFocused,
            this, &HotkeySettingsDialog::onBindingFocused);
    setHub(m_page);  // single-page dialog — page IS the hub

    // Footer hints — each glyph must match the key/button that actually
    // fires the action.
    //
    //   action     keyboard          Xbox    PS         binding
    //   ────────── ───────────────── ─────── ────────── ─────────────────────────────────────────────
    //   confirm    ↵ (Enter)         A       Cross      Key_Return                → Rebind focused
    //   clear      Del               B       Circle     Key_Delete / Key_Back     → Restore Default
    //   auto_map   M                 Y       Triangle   Key_M                     → Reset All
    //   close      Esc               X       Square     Key_Backspace / Key_Escape → close dialog
    //
    // Close is on X/Square (Key_Backspace via SDL's X-button synthesis) — the
    // same button the Controller Mapping page uses — plus Esc. Restore Default
    // stays on B/Circle + Del, so nothing is lost by moving Backspace off it.
    if (m_descBar) {
        m_descBar->setInputManager(inputManager);
        m_descBar->setHints({
            { QStringLiteral("confirm"),  QStringLiteral("Rebind") },
            { QStringLiteral("clear"),    QStringLiteral("Restore Default") },
            { QStringLiteral("auto_map"), QStringLiteral("Reset All") },
            { QStringLiteral("close"),    QStringLiteral("Exit") },
        });
    }

    // Window-context shortcuts for the footer actions. Bypasses the
    // focus tree, so they fire reliably on both the dual-column libretro
    // instance and the single-column per-emulator instances regardless
    // of which child widget owns focus. Gated on isCapturing() so the
    // user can still bind these keys as native hotkey keys.
    auto* mShortcut = new QShortcut(QKeySequence(Qt::Key_M), this);
    mShortcut->setContext(Qt::WindowShortcut);
    connect(mShortcut, &QShortcut::activated, this, [this]{
        if (m_page && !m_page->isCapturing()) m_page->restoreDefaults();
    });

    auto* backShortcut = new QShortcut(QKeySequence(Qt::Key_Back), this);
    backShortcut->setContext(Qt::WindowShortcut);
    connect(backShortcut, &QShortcut::activated, this, [this]{
        if (m_page && !m_page->isCapturing()) m_page->restoreFocusedToDefault();
    });

    auto* delShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    delShortcut->setContext(Qt::WindowShortcut);
    connect(delShortcut, &QShortcut::activated, this, [this]{
        if (m_page && !m_page->isCapturing()) m_page->restoreFocusedToDefault();
    });

    // Backspace = X/Square face button (SDL synth) → close, matching the
    // Controller Mapping page. Gated on !isCapturing so the user can still
    // bind Backspace itself as a native hotkey.
    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    backspaceShortcut->setContext(Qt::WindowShortcut);
    connect(backspaceShortcut, &QShortcut::activated, this, [this]{
        if (m_page && !m_page->isCapturing()) accept();
    });
}

void HotkeySettingsDialog::onBindingFocused(HotkeyDef def, QString currentDisplay) {
    if (m_descBar) m_descBar->setHotkey(def, currentDisplay);
}

void HotkeySettingsDialog::onRestoreDefaultsClicked() {
    if (m_page) m_page->restoreDefaults();
}

void HotkeySettingsDialog::keyPressEvent(QKeyEvent* e) {
    if (!m_page) {
        EmulatorSettingsDialogBase::keyPressEvent(e);
        return;
    }
    // Footer-action shortcuts. Most dispatch is via window-context
    // QShortcuts (above) which fire before the focus tree. This keyPressEvent
    // is the fallback path; keep it in sync with the QShortcuts and the
    // hint-glyph table.
    //
    // SDL face-button → Qt key map (CLAUDE.md):
    //   A → Return, B → Key_Back, X → Backspace, Y → M
    switch (e->key()) {
        case Qt::Key_Return:                            // Enter / A button
            m_page->rebindFocused();
            return;
        case Qt::Key_Back:                              // B button (controller)
        case Qt::Key_Delete:                            // Del (keyboard)
            m_page->restoreFocusedToDefault();
            return;
        case Qt::Key_M:                                 // M / Y button
            m_page->restoreDefaults();
            return;
        case Qt::Key_Backspace:                         // ⌫ / X / Square (SDL synth)
        case Qt::Key_Escape:                            // keyboard Esc
            accept();                                   // close dialog
            return;
        default:
            break;
    }
    EmulatorSettingsDialogBase::keyPressEvent(e);
}

void HotkeySettingsDialog::showEvent(QShowEvent* e) {
    EmulatorSettingsDialogBase::showEvent(e);
    // Auto-focus the first hotkey row so keyboard / controller navigation
    // works immediately. Deferred via singleShot(0, ...) for the same
    // reason as the base class's own auto-focus: Qt's showEvent does its
    // own focus shuffling on the same tick.
    GenericHotkeyPage* page = m_page;
    if (page) QTimer::singleShot(0, page, [page]{ page->focusFirstRow(); });
}
