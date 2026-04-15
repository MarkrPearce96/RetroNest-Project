#pragma once
#include <QColor>
#include <QString>

// DuckStation Settings dialog palette. Initially identical to Pcsx2Theme per
// spec 2026-04-15 — divergence is deferred until per-page visual tweaks.
namespace DuckStationTheme {

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

} // namespace DuckStationTheme
