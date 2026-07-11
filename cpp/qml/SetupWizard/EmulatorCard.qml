import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: root
    width: 140
    height: 140

    property string emuId: ""
    property string emuName: ""
    property string systems: ""
    property bool selected: false
    property bool isFocused: false

    signal clicked()

    function logoForEmu(id) {
        var logos = {
            "pcsx2": "qrc:/SetupWizard/qml/AppUI/images/pcsx2_logo.png",
            "duckstation": "qrc:/SetupWizard/qml/AppUI/images/duckstation_logo.png",
            "ppsspp": "qrc:/SetupWizard/qml/AppUI/images/ppsspp_logo.png",
            "dolphin": "qrc:/SetupWizard/qml/AppUI/images/dolphin_logo.png",
            "mgba": "qrc:/SetupWizard/qml/AppUI/images/mgba_logo.png"
        }
        return logos[id] || ""
    }

    // Glow border when selected or focused
    Rectangle {
        anchors.fill: card
        anchors.margins: -4
        radius: card.radius + 4
        color: "transparent"
        border.width: root.isFocused ? 2 : 1
        border.color: Qt.rgba(WizardTheme.accent.r, WizardTheme.accent.g, WizardTheme.accent.b,
                              root.isFocused ? 0.6 : 0.3)
        opacity: (root.selected || root.isFocused) ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: WizardTheme.animNormal } }
        Behavior on border.color { ColorAnimation { duration: WizardTheme.animNormal } }
        Behavior on border.width { NumberAnimation { duration: WizardTheme.animNormal } }
    }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: 12
        color: root.selected ? WizardTheme.cardSelected : WizardTheme.surface
        border.width: (root.isFocused || root.selected) ? 2 : 1
        border.color: (root.isFocused || root.selected) ? WizardTheme.accent : WizardTheme.surfaceBorder
        clip: true

        Behavior on color { ColorAnimation { duration: WizardTheme.animNormal } }
        Behavior on border.color { ColorAnimation { duration: WizardTheme.animNormal } }
        Behavior on border.width { NumberAnimation { duration: WizardTheme.animNormal } }

        Image {
            id: logoImg
            anchors.centerIn: parent
            width: parent.width - 24
            height: parent.height - 24
            source: logoForEmu(root.emuId)
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            visible: false
        }

        Rectangle {
            id: logoMask
            anchors.fill: logoImg
            radius: 10
            visible: false
        }

        OpacityMask {
            anchors.fill: logoImg
            source: logoImg
            maskSource: logoMask
        }

        // Fallback: show name if no logo
        Text {
            anchors.centerIn: parent
            text: root.emuName
            color: WizardTheme.textPrimary
            font.pixelSize: 14
            font.weight: Font.Bold
            visible: logoForEmu(root.emuId) === ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width - 16
        }
    }

    // Selected check badge — same badge language as PillButton's selection chips
    Rectangle {
        visible: root.selected
        width: 20
        height: 20
        radius: 10
        color: WizardTheme.accent
        anchors.right: card.right
        anchors.top: card.top
        anchors.rightMargin: -6
        anchors.topMargin: -6

        opacity: root.selected ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: WizardTheme.animNormal } }

        Text {
            anchors.centerIn: parent
            text: "✓"
            color: WizardTheme.textPrimary
            font.pixelSize: 12
            font.weight: Font.Bold
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        onPressed: card.scale = 0.97
        onReleased: card.scale = 1.0
    }

    Behavior on scale { NumberAnimation { duration: 100 } }
}
