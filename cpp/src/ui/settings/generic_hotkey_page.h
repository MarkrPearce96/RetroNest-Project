#pragma once

#include <QWidget>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "core/binding_def.h"

class SdlInputManager;
class AppController;
class HotkeyBindingRow;

// Schema-driven hotkey page. Reads `AppController::hotkeyBindings(emuId)`
// once at construction, groups entries by `HotkeyDef::group` (preserving
// adapter declaration order), and renders one section header + one
// HotkeyBindingRow per entry. Owns the capture state machine that was
// previously embedded in HotkeySettingsPage.
class GenericHotkeyPage : public QWidget {
    Q_OBJECT
public:
    GenericHotkeyPage(SdlInputManager* inputManager,
                      AppController* appController,
                      const QString& emuId,
                      QWidget* parent = nullptr);

    bool isEmpty() const { return m_entries.isEmpty(); }

    // Public action API — called from the hosting dialog's face-button
    // shortcuts. All operate on the currently focused row, no-op when
    // there is no focus.
    void rebindFocused();
    void appendRebindFocused();
    void clearFocused();
    void restoreFocusedToDefault();
    void restoreDefaults();

    // Focus the first hotkey row (called by the hosting dialog on showEvent
    // so keyboard / controller navigation works without a mouse-click priming).
    // No-op when the page has no entries (Dolphin / empty-state).
    void focusFirstRow();

signals:
    // Emitted when a row gains focus. `currentDisplay` is the formatted
    // display string ("Period", "SDL-0 R1 + SDL-0 Circle", or empty).
    void bindingFocused(HotkeyDef def, QString currentDisplay);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void buildLayout();
    void loadBindings();

    void onRowFocused(const HotkeyDef& def);
    void navigateFromRow(int direction);
    void startCapture(const HotkeyDef& def, bool append);
    void stopCapture(bool save);
    void onBindingCaptured(int deviceIndex, const QString& element,
                            bool isAxis, bool positive);
    void onKeyboardCaptured(const QString& keyString);
    void finishCapture(const QString& formatted);

    QString currentDisplayFor(const QString& iniKey) const;

    SdlInputManager* m_inputManager;
    AppController* m_appController;
    QString m_emuId;

    QVector<HotkeyDef> m_entries;             // adapter declaration order
    QHash<QString, QString> m_currentValues;  // INI key -> raw stored value
    QHash<QString, HotkeyBindingRow*> m_rowByKey;

    HotkeyBindingRow* m_focusedRow = nullptr;

    QString m_capturingKey;
    bool m_appendMode = false;
    QTimer* m_captureTimer = nullptr;
    int m_captureCountdown = 0;
    QStringList m_capturedBindings;
    // "Quick tap = single bind, hold = multi-bind" semantics: capture
    // commits when every input that was pressed during the session is
    // released. Tracks held keyboard keys (Qt key codes) and whether SDL
    // reports any controller buttons currently held.
    QSet<int> m_heldKeyboardKeys;
    bool      m_controllerHeld = false;

    // Commit the capture if no inputs are still held.
    void maybeCommitOnRelease();
};
