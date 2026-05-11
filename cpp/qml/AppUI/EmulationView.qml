import QtQuick
import QtQuick.Controls
import AppUI

/**
 * EmulationView — fullscreen page for in-process (libretro) games.
 *
 * Pushed onto mainStack when a libretro game starts. It:
 *   • Hosts LibretroVideoItem and pipes frameReady frames to it.
 *   • Forwards Escape / Key_Back to the in-game menu (same hotkey as
 *     the global Cmd+Shift+Escape path in AppWindow).
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

    // Loader switches between software (LibretroVideoItem) and Metal
    // (LibretroMetalItem) based on the active core's backend preference.
    Loader {
        id: videoLoader
        anchors.fill: parent
        sourceComponent: (root.session && root.session.libretroBackend === "metal")
            ? metalComponent
            : softwareComponent

        Component {
            id: softwareComponent
            LibretroVideoItem {
                anchors.fill: parent
                aspectMode:   root.session ? root.session.libretroAspectMode   : "native"
                integerScale: root.session ? root.session.libretroIntegerScale  : false
            }
        }

        Component {
            id: metalComponent
            LibretroMetalItem {
                anchors.fill: parent
                Component.onCompleted: {
                    if (root.session)
                        root.session.registerHardwareView(nativeView())
                }
                Component.onDestruction: {
                    if (root.session)
                        root.session.registerHardwareView(0)
                }
            }
        }
    }

    // Route frameReady from the GameSession to the active video item.
    // Connections.target may be null before a game starts — Qt silently
    // ignores a null target, so no guard is needed.
    // LibretroMetalItem.setFrame is a no-op (frames go via CAMetalLayer directly).
    Connections {
        target: root.session
        function onFrameReady(frame) {
            if (videoLoader.item && videoLoader.item.setFrame)
                videoLoader.item.setFrame(frame)
        }
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
