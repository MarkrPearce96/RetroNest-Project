import QtQuick
import QtQuick.Controls
import AppUI

/**
 * EmulationView — fullscreen page for in-process (libretro) games.
 *
 * Pushed onto mainStack when a libretro game starts. It:
 *   • Hosts LibretroVideoItem and pipes frameReady frames to it.
 *   • Forwards Escape / Key_Back to the in-game menu (same hotkey as
 *     the global Cmd+Escape path in AppWindow).
 *
 * The existing process-emulator path (PCSX2, DuckStation, Dolphin, PPSSPP)
 * never pushes this view — it remains unaffected.
 */
Item {
    id: root
    anchors.fill: parent

    /** Sentinel used by AppWindow to identify this page in the StackView. */
    readonly property bool isEmulationView: true

    /**
     * Bound by AppWindow to app.gameSession so that the Connections
     * block can route frameReady without QML knowing about C++ types.
     */
    property var session: null

    /** Re-emitted when the user wants the in-game menu overlay. */
    signal inGameMenuRequested()

    // Black letterbox behind the video frame
    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    LibretroVideoItem {
        id: video
        anchors.fill: parent
    }

    // Route frameReady from the GameSession to the video item.
    // Connections.target may be null before a game starts — Qt silently
    // ignores a null target, so no guard is needed.
    Connections {
        target: root.session
        function onFrameReady(frame) { video.setFrame(frame) }
    }

    // Keyboard: Escape or B-button (Key_Back) opens the in-game menu
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            root.inGameMenuRequested()
            event.accepted = true
        }
    }

    Component.onCompleted: forceActiveFocus()
}
