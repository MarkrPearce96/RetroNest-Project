pragma Singleton
import QtQuick

QtObject {
    // Colors — warm charcoal + amber
    readonly property color background:     "#131210"
    readonly property color surface:        "#201f1c"
    readonly property color surfaceHover:   "#2e2c28"
    readonly property color accent:         "#e8a838"
    readonly property color accentLight:    "#f0b848"
    readonly property color navBackground:  "#1a1917"
    readonly property color cardSelected:   "#2a2518"
    readonly property color textPrimary:    "#e0ddd6"
    readonly property color textSecondary:  "#c8c4b8"
    readonly property color textMuted:      "#8a8680"
    readonly property color textDim:        "#6a6660"
    readonly property color divider:        "#2e2c28"
    readonly property color success:        "#6a9b4a"
    readonly property color error:          "#c85040"

    // Sizes
    readonly property int pageMargin: 48
    readonly property int pageTopMargin: 40
    readonly property int cardWidth: 160
    readonly property int cardHeight: 110
    readonly property int cardRadius: 14
    readonly property int cardSpacing: 16
    readonly property int pillWidth: 120
    readonly property int pillHeight: 50
    readonly property int pillRadius: 25
    readonly property int navHeight: 64

    // Animation
    readonly property int animFast: 150
    readonly property int animNormal: 200
    readonly property int animSlow: 300
}
