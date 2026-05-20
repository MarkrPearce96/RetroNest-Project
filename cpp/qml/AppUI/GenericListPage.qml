import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    focus: true

    // --- Required ---
    property alias model: listView.model
    required property Component delegate

    // --- Optional content ---
    property string headerText: ""
    property string emptyText: ""
    property Component listFooter: null

    // --- Optional layout knobs ---
    property real listMargins: 20
    property real itemSpacing: 6

    // --- Signals ---
    signal activated(int index)
    signal backRequested()

    // Public API for delegate click handlers
    function activate(index) {
        listView.currentIndex = index
        activated(index)
    }

    // Header
    Text {
        id: headerLabel
        visible: root.headerText.length > 0
        text: root.headerText
        color: SettingsTheme.text
        font.pixelSize: 18
        font.weight: Font.Bold
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.leftMargin: root.listMargins
    }

    // List
    ListView {
        id: listView
        anchors.top: headerLabel.visible ? headerLabel.bottom : parent.top
        anchors.topMargin: headerLabel.visible ? 16 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: root.listMargins
        anchors.bottomMargin: root.listMargins
        spacing: root.itemSpacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        focus: true
        currentIndex: 0
        delegate: root.delegate
        footer: root.listFooter

        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
    }

    // Empty state
    Text {
        anchors.centerIn: listView
        visible: root.emptyText.length > 0 && listView.count === 0
        text: root.emptyText
        color: SettingsTheme.textDim
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
    }

    // Keyboard navigation
    Keys.onUpPressed: {
        if (listView.count > 0)
            listView.currentIndex = listView.currentIndex > 0
                ? listView.currentIndex - 1
                : listView.count - 1
    }
    Keys.onDownPressed: {
        if (listView.count > 0)
            listView.currentIndex = listView.currentIndex < listView.count - 1
                ? listView.currentIndex + 1
                : 0
    }
    Keys.onReturnPressed: if (listView.count > 0) root.activate(listView.currentIndex)
    Keys.onEnterPressed:  if (listView.count > 0) root.activate(listView.currentIndex)
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            event.accepted = true
            root.backRequested()
        }
    }

    // Default back handler — pops panelStack if available
    onBackRequested: {
        if (typeof panelStack !== 'undefined' && panelStack.depth > 1)
            panelStack.pop()
    }
}
