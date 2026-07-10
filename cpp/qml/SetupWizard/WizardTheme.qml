pragma Singleton
import QtQuick

QtObject {
    // Colors — Sunset premium (B2)
    readonly property color background:     "#241033"   // gradient tail / fallback
    readonly property color gradTop:        "#ff5e8a"
    readonly property color gradMid:        "#7a2b6b"
    readonly property color gradBottom:     "#241033"
    readonly property color surface:        "#14ffffff" // glass fill (white ~8%)
    readonly property color surfaceHover:   "#22ffffff"
    readonly property color surfaceBorder:  "#2bffffff"
    readonly property color accent:         "#ff5e8a"
    readonly property color accentLight:    "#ffb057"
    readonly property color navBackground:  "transparent"
    readonly property color cardSelected:   "#26ffffff"
    readonly property color textPrimary:    "#fff5f0"
    readonly property color textSecondary:  "#f2c9d8"
    readonly property color textMuted:      "#e7b7c7"
    readonly property color textDim:        "#ffd0a6"   // step labels
    readonly property color divider:        "#1fffffff"
    readonly property color success:        "#3ec6a0"
    readonly property color error:          "#ff6b6b"
    readonly property color ctaBg:          "#fff5f0"
    readonly property color ctaText:        "#3a1230"

    // Sizes
    readonly property int pageMargin: 72
    readonly property int pageTopMargin: 52
    readonly property int cardWidth: 160
    readonly property int cardHeight: 110
    readonly property int cardRadius: 14
    readonly property int cardSpacing: 16
    readonly property int pillWidth: 120
    readonly property int pillHeight: 56
    readonly property int pillRadius: 28
    readonly property int navHeight: 64

    // Animation
    readonly property int animFast: 150
    readonly property int animNormal: 200
    readonly property int animSlow: 300
}
