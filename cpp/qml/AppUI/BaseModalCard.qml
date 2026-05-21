import QtQuick

Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 150

    default property alias cardContent: card.data
    property int cardWidth: 400
    property real cardHeight: 0      // > 0: explicit; 0: auto (children's bounding box + 48 padding)
    property bool closeOnScrimClick: true

    signal closeRequested()

    function open() {
        visible = true
        forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)
        MouseArea {
            anchors.fill: parent
            onClicked: { if (root.closeOnScrimClick) root.closeRequested() }
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: root.cardWidth
        height: root.cardHeight > 0 ? root.cardHeight : childrenRect.height + 48
        radius: 12
        color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1
        Behavior on height { NumberAnimation { duration: 150 } }
        // Default-property alias points here (card.data). Caller children
        // land directly in the card Rectangle — their `parent` is the card,
        // so existing anchors like `parent.bottom; bottomMargin: -36` work
        // unchanged. The card does not `clip: true`, so negative-margin
        // overflow (e.g. ButtonHints floating below the card) renders.
    }

    // Only fires for derived modals that do NOT define their own Keys.onPressed.
    // QML attached-property semantics mean a derived Keys.onPressed fully replaces
    // this one — derived modals with custom key handling must call closeRequested()
    // themselves on Esc / Backspace / Back.
    Keys.onPressed: function(event) {
        if (!visible) return
        if (event.key === Qt.Key_Escape
                || event.key === Qt.Key_Backspace
                || event.key === Qt.Key_Back) {
            event.accepted = true
            root.closeRequested()
        }
    }
}
