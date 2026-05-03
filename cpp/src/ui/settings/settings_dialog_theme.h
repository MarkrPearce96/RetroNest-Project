#pragma once
#include <QColor>
#include <QString>

// Shared palette + stylesheet fragments for every per-emulator settings
// dialog (PCSX2, DuckStation, PPSSPP). Spec 2026-04-11 mandates a warm
// mid-grey + amber palette for these dialogs, distinct from the global
// settings_theme.h. Per-emulator divergence has been deferred indefinitely;
// when/if it returns, split this back into per-emulator headers.
namespace SettingsDialogTheme {

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
inline QColor previewCardBg()  { return QColor("#504c48"); }

inline QString cardQss() {
    return QStringLiteral(
        "QFrame#SettingsCard {"
        "  background-color: #646058;"
        "  border: 1px solid #706c66;"
        "  border-radius: 8px;"
        "}"
        "QFrame#SettingsCard[focused=\"true\"] {"
        "  border: 1px solid #f59e0b;"
        "}");
}

inline QString sectionHeaderQss() {
    return QStringLiteral(
        "QLabel#SettingsSectionHeader {"
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
        "}");
}

inline QString sliderQss() {
    return QStringLiteral(
        "QSlider { border: 1px solid transparent; border-radius: 4px; padding: 2px; }"
        "QSlider:focus { border: 1px solid #f59e0b; }"
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
        "QFrame#SettingsDescriptionBar {"
        "  background-color: #4a4642;"
        "  border-left: 3px solid #f59e0b;"
        "  padding: 12px 16px;"
        "}"
        "QLabel#SettingsDescText { color: #f2efe8; font-size: 13px; }"
        "QLabel#SettingsDescRecommended {"
        "  color: #f59e0b; font-size: 12px;"
        "  padding: 3px 8px;"
        "  border: 1px solid rgba(245,158,11,0.25);"
        "  border-radius: 5px;"
        "  background-color: rgba(245,158,11,0.09);"
        "}");
}

} // namespace SettingsDialogTheme
