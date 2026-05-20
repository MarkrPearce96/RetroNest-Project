import QtQuick

/**
 * ActionToast — small pill anchored top-right that appears when an
 * in-game action fires (Save State, Load State, Fast Forward). Two
 * modes:
 *
 *   Transient (sticky == false): show() displays the pill, then it
 *     auto-hides after `duration` ms.
 *   Sticky (sticky == true): show() displays it indefinitely; hide()
 *     dismisses it. Used for the Fast Forward indicator while FF is
 *     on.
 *
 * Slide+fade animation and timer lifecycle come from BaseToast.
 */
BaseToast {
    id: root
    duration: 1500

    property string iconSource: ""
    property string label: ""

    Rectangle {
        id: pill
        width: row.implicitWidth + 24
        height: row.implicitHeight + 16
        radius: 14
        color: Qt.rgba(0.08, 0.08, 0.10, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                source: root.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: root.iconSource !== ""
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.label
                color: "#ffffff"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                visible: root.label !== ""
            }
        }
    }
}
