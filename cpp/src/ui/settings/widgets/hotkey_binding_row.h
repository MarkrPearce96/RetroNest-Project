#pragma once

#include <QWidget>
#include "core/binding_def.h"

class QLabel;
class BindBtn;

// Single hotkey binding row: fixed-width label on the left, BindBtn on
// the right. Emits `focused` on focus-in, plus three input signals that
// the parent page wires to the capture / clear flows.
class HotkeyBindingRow : public QWidget {
    Q_OBJECT
public:
    explicit HotkeyBindingRow(const HotkeyDef& def, QWidget* parent = nullptr);

    const HotkeyDef& def() const { return m_def; }

    // Update the button text. Empty value renders as "Not bound".
    void setBindingDisplay(const QString& displayText);

    // Toggle the "currently capturing" visual style.
    void setCapturing(bool capturing);

    // Replace just the button text while capturing (without recomputing the
    // tooltip). Used by the page to render "<captured> [N]" countdown updates.
    void setCapturingText(const QString& text);

signals:
    void focused(HotkeyDef def);
    void rebindRequested(HotkeyDef def);
    void appendRebindRequested(HotkeyDef def);
    void clearRequested(HotkeyDef def);
    // Emitted on Up/Down while the row has focus. `direction` is +1 for
    // Down, -1 for Up. Page consumes this to move focus to the prev/next
    // row in adapter declaration order — fires before the parent QScrollArea
    // can intercept the arrow key for scrolling.
    void navigateRequested(int direction);

protected:
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    HotkeyDef m_def;
    QLabel* m_label = nullptr;
    BindBtn* m_button = nullptr;
};
