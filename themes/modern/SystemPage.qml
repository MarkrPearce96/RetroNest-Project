import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true

    Component.onCompleted: {
        themeContext.currentFocusedGameId = -1
        root.forceActiveFocus()
        if (systemList.length > 0)
            root.currentArtwork = "assets/artwork/" + systemList[0] + ".webp"
        Qt.callLater(rebuildCarouselModel)
    }
    StackView.onActivated: {
        themeContext.currentFocusedGameId = -1
        root.forceActiveFocus()
    }

    property var hints: [
        {action: "navigate_lr", label: "Browse"},
        {action: "confirm",     label: "Select"},
        {action: "start",       label: "Settings"}
    ]
    property var systemList: themeContext.systems
    property var systemNames: themeContext.systemNames
    property var systemCounts: themeContext.systemGameCounts
    property var favoriteCounts: themeContext.systemFavoriteCounts

    // Which background slot is currently "front"
    property bool artSlotA: true
    property string currentArtwork: ""   // tracks what artwork is currently displayed

    // Re-read when systems or games change
    Connections {
        target: themeContext
        function onSystemsChanged() {
            root.systemList = themeContext.systems
            root.systemNames = themeContext.systemNames
            root.systemCounts = themeContext.systemGameCounts
            root.favoriteCounts = themeContext.systemFavoriteCounts
        }
        function onGamesChanged() {
            root.systemList = themeContext.systems
            root.systemNames = themeContext.systemNames
            root.systemCounts = themeContext.systemGameCounts
            root.favoriteCounts = themeContext.systemFavoriteCounts
        }
    }

    // Keyboard navigation
    Keys.onLeftPressed: root.carouselPrev()
    Keys.onRightPressed: root.carouselNext()
    Keys.onReturnPressed: themeContext.navigateToSystem(systemList[root.carouselIndex])
    Keys.onEnterPressed: themeContext.navigateToSystem(systemList[root.carouselIndex])

    // ─── Background artwork (slot A) ─────────────────────────────────────────
    // artSlotA=true  → A is visible (opacity 1), B is hidden (opacity 0)
    // artSlotA=false → B is visible (opacity 1), A is hidden (opacity 0)
    // When switching: load into the hidden slot, then flip artSlotA once loaded.
    Image {
        id: bgArtA
        anchors.fill: parent
        source: systemList.length > 0 ? "assets/artwork/" + systemList[0] + ".webp" : "assets/artwork/_default.webp"
        fillMode: Image.PreserveAspectCrop
        opacity: root.artSlotA ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 500 } }

        onStatusChanged: {
            if (status === Image.Error) {
                source = "assets/artwork/_default.webp"
                return
            }
            if (status === Image.Ready && !root.artSlotA) {
                root.artSlotA = true
            }
        }
    }

    // ─── Background artwork (slot B) ─────────────────────────────────────────
    Image {
        id: bgArtB
        anchors.fill: parent
        source: ""
        fillMode: Image.PreserveAspectCrop
        opacity: root.artSlotA ? 0.0 : 1.0
        Behavior on opacity { NumberAnimation { duration: 500 } }

        onStatusChanged: {
            if (status === Image.Error) {
                source = "assets/artwork/_default.webp"
                return
            }
            if (status === Image.Ready && root.artSlotA) {
                root.artSlotA = false
            }
        }
    }

    // ─── Dark gradient overlay ────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.15) }
            GradientStop { position: 0.45; color: Qt.rgba(0, 0, 0, 0.25) }
            GradientStop { position: 0.75; color: Qt.rgba(0, 0, 0, 0.65) }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.88) }
        }
    }

    // ─── System info text (bottom-left, above carousel) ──────────────────────
    Column {
        id: systemInfo
        anchors.left: parent.left
        anchors.leftMargin: 60
        anchors.bottom: carouselContainer.top
        anchors.bottomMargin: 16
        spacing: 6
        visible: systemList.length > 0

        Text {
            id: systemNameText
            text: systemList.length > 0
                  ? (systemNames[systemList[root.carouselIndex]] || systemList[root.carouselIndex])
                  : ""
            color: "#ffffff"
            font.pixelSize: 42
            font.weight: Font.Bold
        }

        Text {
            id: systemCountText
            text: systemList.length > 0
                  ? (systemCounts[systemList[root.carouselIndex]] || 0) + " Games  (" + (favoriteCounts[systemList[root.carouselIndex]] || 0) + " Favourites)"
                  : ""
            color: Qt.rgba(1, 1, 1, 0.75)
            font.pixelSize: 16
        }
    }

    // ─── Bottom carousel (repeating systems to fill the row) ────────────────
    property real cardWidth: 320
    property real cardHeight: 240
    property real cardSpacing: 20
    property real selectedScale: 1.35
    property int carouselIndex: 0  // tracks real system index (0..systemList.length-1)

    // Multiplied model: repeat systems enough times to fill the screen
    ListModel { id: carouselModel }

    function rebuildCarouselModel() {
        carouselModel.clear()
        if (systemList.length === 0) return
        var repeatCount = Math.max(Math.ceil(20 / systemList.length), 5)
        for (var r = 0; r < repeatCount; r++) {
            for (var i = 0; i < systemList.length; i++) {
                carouselModel.append({ sysId: systemList[i], realIndex: i })
            }
        }
        carousel.currentIndex = 0
    }

    onSystemListChanged: rebuildCarouselModel()

    // Override keyboard/controller to update carouselIndex
    function carouselNext() {
        if (systemList.length === 0) return
        carousel.incrementCurrentIndex()
        carouselIndex = carousel.currentIndex % systemList.length
        updateCarouselArtwork()
    }
    function carouselPrev() {
        if (systemList.length === 0) return
        carousel.decrementCurrentIndex()
        carouselIndex = ((carousel.currentIndex % systemList.length) + systemList.length) % systemList.length
        updateCarouselArtwork()
    }
    function updateCarouselArtwork() {
        var sysId = systemList[carouselIndex]
        var newSource = "assets/artwork/" + sysId + ".webp"

        // Skip if already showing this artwork
        if (newSource === root.currentArtwork) return
        root.currentArtwork = newSource

        if (root.artSlotA) {
            // A is visible, load into B
            if (bgArtB.source === newSource && bgArtB.status === Image.Ready) {
                // B already has this image cached — just flip
                root.artSlotA = false
            } else {
                bgArtB.source = ""          // force reset so re-setting triggers onStatusChanged
                bgArtB.source = newSource
            }
        } else {
            // B is visible, load into A
            if (bgArtA.source === newSource && bgArtA.status === Image.Ready) {
                root.artSlotA = true
            } else {
                bgArtA.source = ""
                bgArtA.source = newSource
            }
        }
    }

    // ─── Bottom carousel ────────────────────────────────────────────────────
    Item {
        id: carouselContainer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        height: cardHeight * selectedScale + 40
        visible: systemList.length > 0

        PathView {
            id: carousel
            anchors.fill: parent
            model: carouselModel
            pathItemCount: Math.ceil(root.width / (root.cardWidth + root.cardSpacing)) + 2
            preferredHighlightBegin: 0.0
            preferredHighlightEnd: 0.0
            highlightRangeMode: PathView.StrictlyEnforceRange
            highlightMoveDuration: 300
            clip: false
            interactive: true

            path: Path {
                startX: 60 + (root.cardWidth * root.selectedScale) / 2
                startY: (root.cardHeight * root.selectedScale) / 2 + 10

                PathLine {
                    x: 60 + (root.cardWidth * root.selectedScale) / 2 + carousel.pathItemCount * (root.cardWidth + root.cardSpacing)
                    y: (root.cardHeight * root.selectedScale) / 2 + 10
                }
            }

            delegate: Item {
                id: card
                width: root.cardWidth
                height: root.cardHeight + 30

                required property string sysId
                required property int realIndex
                required property int index

                property bool isCurrent: PathView.isCurrentItem
                property int distFromCurrent: {
                    var diff = card.index - carousel.currentIndex
                    var count = carouselModel.count
                    // Normalize to 0..count-1 (positive distance ahead)
                    return ((diff % count) + count) % count
                }

                scale: isCurrent ? root.selectedScale : 1.0
                Behavior on scale { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }
                z: isCurrent ? 10 : 1
                transform: Translate {
                    // Shift all non-selected cards left by 30px to tuck the first one
                    // slightly behind the selected card, keeping gaps consistent
                    x: card.isCurrent ? 0 : (card.distFromCurrent > 0 && card.distFromCurrent < carouselModel.count / 2 ? -30 : 0)
                    Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }
                }

                opacity: isCurrent ? 1.0 : 0.6
                Behavior on opacity { NumberAnimation { duration: 200 } }

                // Logo fills the card directly (no background rectangle)
                Image {
                    id: logoImg
                    width: root.cardWidth
                    height: root.cardHeight
                    source: "assets/logos/" + card.sysId + ".webp"
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: root.cardWidth
                    sourceSize.height: root.cardHeight
                    opacity: card.isCurrent ? 1.0 : 0.35
                    visible: status !== Image.Error && status !== Image.Null
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }

                // Text fallback
                Text {
                    anchors.centerIn: logoImg
                    text: root.systemNames[card.sysId] || card.sysId
                    color: "#ffffff"
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    opacity: card.isCurrent ? 1.0 : 0.5
                    visible: !logoImg.visible
                }

                // System name below card
                Text {
                    anchors.horizontalCenter: logoImg.horizontalCenter
                    anchors.top: logoImg.bottom
                    anchors.topMargin: 6
                    text: root.systemNames[card.sysId] || card.sysId
                    color: card.isCurrent ? "#ffffff" : Qt.rgba(1, 1, 1, 0.5)
                    font.pixelSize: 13
                    font.weight: card.isCurrent ? Font.DemiBold : Font.Normal
                }

                MouseArea {
                    anchors.fill: logoImg
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (card.isCurrent) {
                            themeContext.navigateToSystem(card.sysId)
                        } else {
                            carousel.currentIndex = card.index
                            root.carouselIndex = card.realIndex
                            root.updateCarouselArtwork()
                        }
                    }
                    onDoubleClicked: {
                        themeContext.navigateToSystem(card.sysId)
                    }
                }
            }
        }
    }

}
