#pragma once
#include <QColor>
#include <QString>

// PCSX2 Settings dialog palette — spec 2026-04-11. Do NOT reuse
// settings_theme.h constants; the spec mandates a warm mid-grey +
// amber palette that differs from the global settings theme.
namespace Pcsx2Theme {

inline QColor windowBg()       { return QColor("#585450"); }
inline QColor titleBarBg()     { return QColor("#4a4642"); }
inline QColor cardBg()         { return QColor("#646058"); }
inline QColor cardBorder()     { return QColor("#706c66"); }
inline QColor cardBorderFocus(){ return QColor("#f59e0b"); }
inline QColor inputBg()        { return QColor("#585450"); }
inline QColor textPrimary()    { return QColor("#f2efe8"); }
inline QColor textSecondary()  { return QColor("#d0ccc4"); }
inline QColor textMuted()      { return QColor("#9a9690"); }
inline QColor accent()         { return QColor("#f59e0b"); }
inline QColor letterbox()      { return QColor("#3a3632"); }

// Ready-to-use stylesheet fragments
inline QString cardQss() {
    return QStringLiteral(
        "QFrame#Pcsx2Card {"
        "  background-color: #646058;"
        "  border: 1px solid #706c66;"
        "  border-radius: 8px;"
        "}"
        "QFrame#Pcsx2Card[focused=\"true\"] {"
        "  border: 1px solid #f59e0b;"
        "}");
}

inline QString sectionHeaderQss() {
    return QStringLiteral(
        "QLabel#Pcsx2SectionHeader {"
        "  color: #f59e0b;"
        "  font-size: 12px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  padding-bottom: 4px;"
        "}");
}

inline QString comboQss() {
    return QStringLiteral(
        "QComboBox {"
        "  background-color: #585450;"
        "  color: #f2efe8;"
        "  border: 1px solid #706c66;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  min-height: 24px;"
        "}"
        "QComboBox:focus {"
        "  border: 1px solid #f59e0b;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 22px;"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "}"
        "QComboBox::down-arrow {"
        "  width: 10px;"
        "  height: 10px;"
        "}"
        /* --- popup container --- */
        "QComboBox QAbstractItemView {"
        "  background-color: #585450;"
        "  color: #f2efe8;"
        "  border: 1px solid #706c66;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  margin: 0;"
        "  outline: none;"
        "  selection-background-color: #f59e0b;"
        "  selection-color: #1a1816;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  padding: 6px 12px;"
        "  min-height: 28px;"
        "  border-radius: 4px;"
        "}"
        "QComboBox QAbstractItemView::item:selected {"
        "  background-color: #f59e0b;"
        "  color: #1a1816;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "  background-color: #6c6860;"
        "  color: #f2efe8;"
        "}"
        /* --- scroll bar inside popup --- */
        "QComboBox QAbstractItemView QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 8px;"
        "  margin: 2px;"
        "}"
        "QComboBox QAbstractItemView QScrollBar::handle:vertical {"
        "  background: #706c66;"
        "  border-radius: 4px;"
        "  min-height: 24px;"
        "}"
        "QComboBox QAbstractItemView QScrollBar::add-line:vertical,"
        "QComboBox QAbstractItemView QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}");
}

inline QString sliderQss() {
    return QStringLiteral(
        "QSlider::groove:horizontal {"
        "  height: 4px; background: #4a4642; border-radius: 2px;"
        "}"
        "QSlider::sub-page:horizontal { background: #f59e0b; border-radius: 2px; }"
        "QSlider::handle:horizontal {"
        "  background: #f59e0b; width: 14px; height: 14px;"
        "  margin: -5px 0; border-radius: 7px;"
        "}");
}

inline QString descriptionBarQss() {
    return QStringLiteral(
        "QFrame#Pcsx2DescriptionBar {"
        "  background-color: #4a4642;"
        "  border-left: 3px solid #f59e0b;"
        "  padding: 12px 16px;"
        "}"
        "QLabel#Pcsx2DescText { color: #f2efe8; font-size: 13px; }"
        "QLabel#Pcsx2DescRecommended {"
        "  color: #f59e0b; font-size: 12px;"
        "  padding: 3px 8px;"
        "  border: 1px solid rgba(245,158,11,0.25);"
        "  border-radius: 5px;"
        "  background-color: rgba(245,158,11,0.09);"
        "}");
}

} // namespace Pcsx2Theme
