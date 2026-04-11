#pragma once
#include <QFrame>

// Base card for the PCSX2 settings grid. Keyboard-focusable; repaints
// a 2 px amber halo when focused (QSS cannot draw outer glow).
class Pcsx2Card : public QFrame {
    Q_OBJECT
    Q_PROPERTY(bool focused READ hasFocus NOTIFY focusedChanged)
public:
    explicit Pcsx2Card(QWidget* parent = nullptr);

signals:
    void focused();
    void focusedChanged();
    void activated(); // Enter / Return

protected:
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
};
