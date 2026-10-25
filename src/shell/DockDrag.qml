// A full-screen, input-less overlay showing a dragged dock tile's icon, and
// the theme's removal animation (dock.remove_effect) where a tile is dropped.

import QtQuick
import Quickshell
import Quickshell.Wayland

PanelWindow {
    id: drag

    required property var looks
    required property string themeDirectory

    // pointX/pointY: where the icon's middle is on the screen.
    property string iconSource: ""
    property real iconSize: 0
    property real pointX: 0
    property real pointY: 0

    readonly property var effectLooks: looks.remove_effect ?? null

    function showRemoval(x, y) {
        if (effectLooks === null) {
            return
        }
        effect.x = Math.round(x - effect.width / 2)
        effect.y = Math.round(y - effect.height / 2)
        showsEffect = true
        effect.restart()
    }

    property bool showsEffect: false

    visible: iconSource !== "" || showsEffect

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    color: "transparent"
    mask: Region {}

    Image {
        visible: drag.iconSource !== ""
        x: Math.round(drag.pointX - width / 2)
        y: Math.round(drag.pointY - height / 2)
        width: drag.iconSize
        height: drag.iconSize
        source: drag.iconSource
        sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
    }

    // A vertical strip of frames, clipped and stepped by a timer.
    // AnimatedSprite is not used: it flashes its first frame at the end.
    Item {
        id: effect
        visible: drag.showsEffect
        readonly property real size: drag.effectLooks?.size ?? 64
        readonly property int frameCount: drag.effectLooks?.frames ?? 1
        property int frame: 0
        width: size
        height: size
        clip: true

        function restart() {
            frame = 0
            nextFrame.restart()
        }

        Image {
            width: effect.size
            height: effect.size * effect.frameCount
            y: -effect.frame * effect.size
            source: drag.effectLooks !== null ? "file://" + drag.themeDirectory + "/" + drag.effectLooks.image : ""
        }
        Timer {
            id: nextFrame
            interval: drag.effectLooks?.frame_ms ?? 50
            repeat: true
            onTriggered: {
                if (effect.frame + 1 < effect.frameCount) {
                    effect.frame += 1
                } else {
                    stop()
                    drag.showsEffect = false
                }
            }
        }
    }
}
