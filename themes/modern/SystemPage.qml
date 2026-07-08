import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true

    Component.onCompleted: {
        themeContext.currentFocusedGameId = -1
        root.forceActiveFocus()
        reconcileArtwork()
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

    // ── Background artwork state ──
    // Which background slot is currently "front"
    property bool artSlotA: true
    // Sources that failed to load (missing per-system artwork) → mapped
    // to the default background instead of retrying forever.
    property var _artworkFailed: ({})

    // DERIVED from the selection — the reconcile loop below moves the
    // visible slot toward this and self-corrects on stale or failed
    // loads. (The old imperative updateCarouselArtwork tracked "what I
    // asked for" separately from "what's actually visible"; a missing
    // file like wii.webp desynced them permanently, which is why systems
    // showed the wrong or default background until re-scrolled.)
    readonly property string desiredArtwork: systemList.length > 0
        ? "assets/artwork/" + systemList[carouselIndex] + ".webp"
        : "assets/artwork/_default.webp"
    onDesiredArtworkChanged: artworkSettle.restart()

    // Debounce so a fast swipe doesn't decode every card it passes.
    Timer {
        id: artworkSettle
        interval: 120
        onTriggered: root.reconcileArtwork()
    }

    function reconcileArtwork() {
        var target = root._artworkFailed[root.desiredArtwork]
                   ? "assets/artwork/_default.webp" : root.desiredArtwork
        if (root._artworkFailed[target])
            return                            // even the fallback failed — keep what's shown
        var visibleImg = root.artSlotA ? bgArtA : bgArtB
        var hiddenImg  = root.artSlotA ? bgArtB : bgArtA
        if (visibleImg.requested === target)
            return                            // already showing it
        if (hiddenImg.requested === target && hiddenImg.status === Image.Ready) {
            root.artSlotA = !root.artSlotA    // already loaded — just flip
            return
        }
        hiddenImg.requested = target
        hiddenImg.source = target             // aborts any stale in-flight load
    }

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

    // Two-finger trackpad swipe (wheel events with pixel deltas on
    // macOS) drives PathView.offset directly, so the carousel tracks
    // the fingers continuously — browser-style. Trackpad momentum
    // arrives as decaying wheel events, so inertia comes free; once the
    // stream goes quiet, snap the nearest card into the highlight slot.
    // currentIndex (and thus carouselIndex + artwork) follow the offset
    // automatically via StrictlyEnforceRange, same as a click-drag.
    //
    // Coexists with click-drag: while the PathView is being dragged,
    // wheel events (including a prior swipe's momentum tail, which can
    // stream for over a second) are ignored — otherwise the leftover
    // inertia writes fight the drag and it feels broken.
    property real swipeSpeed: 2.0   // finger-to-carousel distance multiplier
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            if (carousel.dragging || carouselModel.count === 0) return
            // Dominant axis, so slightly-diagonal two-finger swipes (and
            // plain mouse scroll wheels) drive the carousel too.
            var px = event.pixelDelta.x !== 0 ? event.pixelDelta.x
                                              : event.angleDelta.x / 8
            var py = event.pixelDelta.y !== 0 ? event.pixelDelta.y
                                              : event.angleDelta.y / 8
            var dx = Math.abs(px) >= Math.abs(py) ? px : py
            if (dx === 0) return
            snapAnim.stop()
            carousel.offset += dx * root.swipeSpeed
                               / (root.cardWidth + root.cardSpacing)
            wheelSettle.restart()
        }
    }
    Timer {
        id: wheelSettle
        interval: 120
        onTriggered: {
            snapAnim.from = carousel.offset
            snapAnim.to = Math.round(carousel.offset)
            if (snapAnim.from !== snapAnim.to) snapAnim.start()
        }
    }
    NumberAnimation {
        id: snapAnim
        target: carousel
        property: "offset"
        duration: 200
        easing.type: Easing.OutCubic
    }

    // Keyboard navigation
    Keys.onLeftPressed: root.carouselPrev()
    Keys.onRightPressed: root.carouselNext()
    Keys.onReturnPressed: themeContext.navigateToSystem(systemList[root.carouselIndex])
    Keys.onEnterPressed: themeContext.navigateToSystem(systemList[root.carouselIndex])

    // ─── Background artwork (two-slot crossfade) ────────────────────────────
    // artSlotA=true  → A is visible (opacity 1), B is hidden (opacity 0)
    // artSlotA=false → B is visible (opacity 1), A is hidden (opacity 0)
    // Slots never flip themselves — every completion (Ready OR Error)
    // reports back to reconcileArtwork(), which alone decides whether to
    // flip, reload, or fall back. `requested` records what was asked of
    // the slot (source read-back is a resolved URL, so it can't be
    // compared against the relative paths we set).
    Image {
        id: bgArtA
        anchors.fill: parent
        property string requested: ""
        fillMode: Image.PreserveAspectCrop
        opacity: root.artSlotA ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 500 } }

        onStatusChanged: {
            if (status === Image.Error)
                root._artworkFailed[requested] = true
            if (status === Image.Error || status === Image.Ready)
                root.reconcileArtwork()
        }
    }

    Image {
        id: bgArtB
        anchors.fill: parent
        property string requested: ""
        fillMode: Image.PreserveAspectCrop
        opacity: root.artSlotA ? 0.0 : 1.0
        Behavior on opacity { NumberAnimation { duration: 500 } }

        onStatusChanged: {
            if (status === Image.Error)
                root._artworkFailed[requested] = true
            if (status === Image.Error || status === Image.Ready)
                root.reconcileArtwork()
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
    // Real system index (0..systemList.length-1), DERIVED from the
    // PathView's currentIndex so it can never desync — the view is
    // interactive, and a flick moves currentIndex without going through
    // carouselNext/Prev (shadow-state copies went stale there, so Enter
    // launched the pre-flick system).
    readonly property int carouselIndex: systemList.length > 0
        ? ((carousel.currentIndex % systemList.length) + systemList.length) % systemList.length
        : 0

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

    // Keyboard/controller navigation; carouselIndex and the background
    // artwork follow via the currentIndex binding + desiredArtwork.
    function carouselNext() {
        if (systemList.length === 0) return
        carousel.incrementCurrentIndex()
    }
    function carouselPrev() {
        if (systemList.length === 0) return
        carousel.decrementCurrentIndex()
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

            // (Background artwork needs no trigger here — desiredArtwork
            // derives from carouselIndex, which derives from currentIndex,
            // so every navigation source updates it automatically.)

            // A click-drag takes over from any in-flight wheel gesture:
            // kill the pending snap so it can't fight the finger.
            onDraggingChanged: {
                if (dragging) {
                    snapAnim.stop()
                    wheelSettle.stop()
                }
            }

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
