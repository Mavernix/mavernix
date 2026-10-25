// A preview of a file's contents in place of its icon, used by the desktop
// icons. `frame` is a theme table { color, width, share, shadow_color,
// shadow_blur, shadow_drop }; the framed picture fits `share` of the square.
// Without a frame the bare picture fills the square.

import QtQuick
import QtQuick.Effects
import "ThemeValues.js" as ThemeValues

Item {
    id: framed

    property url source
    property var frame: null
    // For now: see `retries_left`.
    readonly property bool failed: picture.status === Image.Error

    // A new file may still be being written when it is first read, and
    // nothing says when it is complete, so a failed read is retried.
    readonly property int retry_limit: 6
    readonly property int retry_delay_ms: 500
    property int retries_left: retry_limit
    // An Image does not read the same url twice, but does read one that
    // changes and changes back.
    property bool reloading: false
    onSourceChanged: retries_left = retry_limit

    readonly property real edge: frame?.width ?? 0
    readonly property real room: Math.min(width, height) * (frame?.share ?? 1) - 2 * edge

    RectangularShadow {
        visible: framed.frame?.shadow_color !== undefined && picture.status === Image.Ready
        anchors.fill: edgeBox
        offset.y: framed.frame?.shadow_drop ?? 0
        blur: framed.frame?.shadow_blur ?? 0
        color: ThemeValues.color(framed.frame?.shadow_color ?? "#00000000")
    }
    Rectangle {
        id: edgeBox
        visible: framed.frame !== null && picture.status === Image.Ready
        anchors.centerIn: parent
        width: Math.round(picture.paintedWidth) + 2 * framed.edge
        height: Math.round(picture.paintedHeight) + 2 * framed.edge
        color: ThemeValues.color(framed.frame?.color ?? "#ffffff")
    }
    Image {
        id: picture
        anchors.centerIn: parent
        width: framed.room
        height: framed.room
        source: framed.reloading ? "" : framed.source
        asynchronous: true
        sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
        fillMode: Image.PreserveAspectFit
    }

    Timer {
        id: retryTimer
        interval: framed.retry_delay_ms
        running: picture.status === Image.Error && framed.retries_left > 0
        onTriggered: {
            framed.retries_left -= 1;
            framed.reloading = true;
            framed.reloading = false;
        }
    }
}
