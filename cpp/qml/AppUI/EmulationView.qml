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

    // Loader switches between software (LibretroVideoItem), Metal
    // (LibretroMetalItem — PCSX2 NSView), and GL (LibretroGLItem —
    // PPSSPP via SET_HW_RENDER + IOSurface→MTLTexture import) based on
    // the active core's backend preference.
    Loader {
        id: videoLoader
        anchors.fill: parent
        sourceComponent: {
            if (!root.session) return softwareComponent
            if (root.session.libretroBackend === "metal") return metalComponent
            if (root.session.libretroBackend === "gl")    return glComponent
            return softwareComponent
        }

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
                // Mirror the software path's aspect handling so the HW
                // render bridge doesn't stretch the renderer's output to
                // fill the host window. session.libretroAspectMode is
                // sourced from the libretro adapter's frontend store
                // (defaults to "native"); nativeAspect=4/3 covers the
                // PS2 case (PCSX2 reports av_info.geometry.aspect_ratio
                // = 4/3 via the libretro shell). If a future adapter
                // exposes the av-info aspect dynamically, plumb it
                // through here instead of the 4/3 constant.
                aspectMode: root.session ? root.session.libretroAspectMode : "native"
                // Dynamic — sourced from av_info.geometry.aspect_ratio
                // (PCSX2 reports 4/3; any future core reporting 16:9, 8:7
                // etc. flows through automatically). Falls back to 4/3
                // when the session isn't yet bound.
                nativeAspect: root.session ? root.session.libretroAspectRatio : (4.0 / 3.0)
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

        Component {
            id: glComponent
            LibretroGLItem {
                id: glItem
                anchors.fill: parent
                aspectMode: root.session ? root.session.libretroAspectMode : "native"
                integerScale: root.session ? root.session.libretroIntegerScale : false
                nativeAspect: root.session ? root.session.libretroAspectRatio : (16.0 / 9.0)
                // VideoHardwareGL is created lazily inside CoreRuntime's
                // installHwRender callback during retro_load_game. By the
                // time aboutToStartLibretro fires (which is what pushes
                // this view), session.videoHardware() returns null. We
                // poll on libretroBackendChanged and aspectRatioReported
                // (the latter is emitted right after retro_get_system_av_info,
                // by which point installHwRender has completed and the
                // VideoHardwareGL exists).
                function rewire() {
                    if (root.session)
                        glItem.setVideoHardware(root.session.videoHardware())
                }
                Component.onCompleted: rewire()
                Connections {
                    target: root.session
                    // libretroAspectRatioChanged is the GameSession signal that
                    // fires after CoreRuntime emits aspectRatioReported — by
                    // which point installHwRender has completed and
                    // session.videoHardware() returns non-null.
                    function onLibretroAspectRatioChanged() { glItem.rewire() }
                    function onLibretroBackendChanged()     { glItem.rewire() }
                }
                Component.onDestruction: glItem.setVideoHardware(null)
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

    // Esc / Key_Back are no longer caught here — the libretro hotkey
    // matcher in AppController owns all libretro in-game shortcuts now.
    // Users bind their own ToggleMenu key in the Libretro Hotkeys page
    // (default: Esc). Leaving both paths active caused the event to be
    // handled twice (once via matcher = open, once here via the legacy
    // toggle connection = close).

    Component.onCompleted: forceActiveFocus()
}
