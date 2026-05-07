#pragma once

#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QMouseEvent>
#include <QString>
#include <functional>

// ── Shared colour constants ────────────────────────────────
inline const QString kBg        = "#242440";
inline const QString kBoxColor  = "#2c2c4e";
inline const QString kBoxBorder = "#3a3a60";
inline const QString kAccent    = "#6c5ce7";
inline const QString kTextPrimary   = "#e8e8ff";
inline const QString kTextSecondary = "#a8a8cc";
inline const QString kBtnDefault    = "#353558";
inline const QString kBtnHover      = "#404070";

inline constexpr int kBtnH = 36;

inline const QString kBtnStyle = QStringLiteral(
    "QPushButton { background: %1; color: %2; border: 1px solid %3;"
    "  border-radius: 6px; font-size: 12px; padding: 4px 8px; }"
    "QPushButton:hover { background: %4; }"
).arg(kBtnDefault, kTextPrimary, kBoxBorder, kBtnHover);

inline const QString kCapturingStyle = QStringLiteral(
    "QPushButton { background: %1; color: %2; border: 1px solid %1;"
    "  border-radius: 6px; font-size: 12px; font-style: italic; font-weight: 600;"
    "  padding: 4px 8px; }"
).arg(kAccent, kTextPrimary);

// ── Binding button with right-click ────────────────────────
class BindBtn : public QPushButton {
public:
    std::function<void()> onRightClick;
    std::function<void()> onShiftClick;
    BindBtn(QWidget* p = nullptr) : QPushButton(p) {}
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::RightButton && onRightClick)
            onRightClick();
        else if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ShiftModifier) && onShiftClick)
            onShiftClick();
        else
            QPushButton::mousePressEvent(e);
    }
};

