#pragma once

#include <QWidget>
#include "core/binding_def.h"

class QLabel;
class BindBtn;

// Single hotkey binding row.
//
// Two layout modes:
//   - Single-column (default): label + one binding button. Used by the
//     per-emulator standalone hotkey dialogs (DuckStation / PPSSPP /
//     Dolphin).
//   - Dual-column: label + keyboard button + controller button. Used by
//     the global Libretro Hotkeys page so each action can be bound
//     independently for keyboard and gamepad. Left/Right arrows toggle
//     which column is "active"; the active column is the target of
//     subsequent Rebind / Restore-Default actions.
class HotkeyBindingRow : public QWidget {
    Q_OBJECT
public:
    enum Column { ColKeyboard = 0, ColController = 1 };

    explicit HotkeyBindingRow(const HotkeyDef& def,
                              bool dualColumn = false,
                              QWidget* parent = nullptr);

    const HotkeyDef& def() const { return m_def; }
    bool isDualColumn() const { return m_dualColumn; }
    Column currentColumn() const { return m_currentColumn; }

    // Single-column display (legacy single-column rows).
    void setBindingDisplay(const QString& displayText);

    // Dual-column display: separate text for keyboard column and
    // controller column. No-op when the row isn't dual-column.
    void setDualBindingDisplay(const QString& keyboardText,
                               const QString& controllerText);

    void setCapturing(bool capturing);
    void setCapturingText(const QString& text);

signals:
    void focused(HotkeyDef def);
    void rebindRequested(HotkeyDef def);
    void appendRebindRequested(HotkeyDef def);
    void clearRequested(HotkeyDef def);
    void navigateRequested(int direction);
    // Dual-column rows emit this when Left/Right arrows toggle the
    // active column. Single-column rows never emit it.
    void columnChanged(HotkeyDef def, Column column);

protected:
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    void applyColumnHighlight();

    HotkeyDef m_def;
    bool      m_dualColumn = false;
    Column    m_currentColumn = ColKeyboard;
    QLabel*   m_label = nullptr;
    BindBtn*  m_button = nullptr;         // single-column / keyboard column
    BindBtn*  m_controllerButton = nullptr;  // controller column (dualColumn only)
};
