// One place in the dock: an application, a minimised window, a folder, a
// shell action, or a separator (drawn by the dock). Clicking an application
// raises its window or starts it, bouncing the icon while it starts.

import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland

Item {
    id: tile

    IconSource {
        id: themeIcons
        choices: tile.choices
    }

    // The shell's UserChoices, or null.
    property var choices: null
    required property var entry
    property var minimizedWindow: null
    // For a tile that is neither an application nor a window.
    property string picture: ""
    property bool pictureComing: false
    property string name: ""
    property var command: []
    // Something for the shell to do on a press, e.g. "app_grid".
    property string shellAction: ""
    signal shellActionWanted(string action)
    property string folder: ""
    signal folderWanted()
    required property int margin
    required property var bounce
    // A pressed icon's colours are multiplied by this.
    required property real pressedDarken
    property real smallestAppearScale: 0.1
    // In milliseconds; 0 to appear at once.
    property int appearDuration: 0

    // 0 to 1; the dock fades this tile's shadow and reflection with it.
    property real appeared: 1
    NumberAnimation {
        id: appearAnimation
        target: tile
        property: "appeared"
        from: 0
        to: 1
        duration: tile.appearDuration
        easing.type: Easing.InOutSine
    }
    Component.onCompleted: {
        if (appearDuration > 0) {
            appearAnimation.start()
        }
    }

    // Read by the dock's layers that draw this icon's shadow and reflection.
    readonly property string iconSource: {
        if (pictureComing) {
            return ""
        }
        if (picture !== "") {
            return picture
        }
        if (entry === null) {
            return ""
        }
        return themeIcons.forEntry(entry, "application-x-executable")
    }
    readonly property real iconSize: width - 2 * margin
    readonly property real iconX: x + margin

    readonly property var windows: entry === null ? [] : ToplevelManager.toplevels.values.filter(
        window => DesktopEntries.heuristicLookup(window.appId) === tile.entry)
    readonly property bool running: minimizedWindow === null && windows.length > 0

    // In pixels; the dock moves the tile up by this and the reflection down.
    property real lift: 0

    // Until the first window appears or bounce.longest_ms has passed.
    property bool launching: false
    onRunningChanged: {
        if (running) {
            launching = false
        }
    }

    Timer {
        interval: tile.bounce.longest_ms
        running: tile.launching
        onTriggered: tile.launching = false
    }

    // One bounce is a parabola, repeated while launching and always finished.
    SequentialAnimation {
        id: bounceAnimation
        // [factor, offset] of the tile's size.
        readonly property real peak: tile.bounce.height[0] * tile.height + tile.bounce.height[1]

        NumberAnimation {
            target: tile
            property: "lift"
            to: bounceAnimation.peak
            duration: tile.bounce.half_period_ms
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: tile
            property: "lift"
            to: 0
            duration: tile.bounce.half_period_ms
            easing.type: Easing.InQuad
        }
        onFinished: {
            if (tile.launching) {
                bounceAnimation.start()
            }
        }
    }

    Image {
        id: icon
        visible: tile.iconSource !== ""
        x: tile.margin
        y: tile.height - height
        width: tile.iconSize
        height: tile.iconSize
        source: tile.iconSource
        scale: tile.smallestAppearScale + (1 - tile.smallestAppearScale) * tile.appeared
        opacity: tile.shownShare

        // With magnification the picture is made once at the largest size
        // and scaled; reloading it at every step stalls the dock.
        readonly property real drawnSide: tile.drawnIconSize > 0 ? tile.drawnIconSize : width
        sourceSize.width: drawnSide * Screen.devicePixelRatio
        sourceSize.height: drawnSide * Screen.devicePixelRatio
        // Avoids shimmering when scaled far down.
        mipmap: tile.drawnIconSize > 0

        // Mixing with black by (1 - pressedDarken) multiplies by pressedDarken.
        layer.enabled: press.pressed
        layer.effect: MultiEffect {
            colorization: 1 - tile.pressedDarken
            colorizationColor: "black"
        }
    }

    readonly property string labelText: {
        if (name !== "") {
            return name
        }
        return minimizedWindow !== null ? minimizedWindow.title : (entry?.name ?? "")
    }
    // Set by the dock, which tracks the pointer across all tiles for
    // magnification; a hover handler here would hide it from the dock.
    property bool pointerOver: false
    readonly property bool hovered: pointerOver && press.enabled

    signal menuWanted()

    // While lifted, the dock draws the icon under the pointer instead. Moves
    // are reported through dragMoved; the dock detects the drag's end itself.
    property bool draggable: false
    property bool lifted: false
    // 0 to load the picture at the size it is shown.
    property real drawnIconSize: 0
    signal minimizedWindowWanted(var window)
    signal dragMoved(real windowX, real windowY)
    readonly property real shownShare: lifted ? 0 : appeared

    function activate() {
        if (folder !== "") {
            folderWanted()
            return
        }
        if (shellAction !== "") {
            shellActionWanted(shellAction)
            return
        }
        if (command.length > 0) {
            Quickshell.execDetached(command)
            return
        }
        if (minimizedWindow !== null) {
            // Through the dock: it may be on another desktop.
            minimizedWindowWanted(minimizedWindow)
            return
        }
        if (running) {
            const inView = windows.find(window => !window.minimized)
            if (inView !== undefined) {
                inView.activate()
            } else {
                minimizedWindowWanted(windows[0])
            }
            return
        }
        entry.execute()
        launching = true
        bounceAnimation.start()
    }

    MouseArea {
        id: press
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: tile.entry !== null || tile.minimizedWindow !== null || tile.command.length > 0
                 || tile.shellAction !== "" || tile.folder !== ""

        // In pixels.
        readonly property real dragStartDistance: 8
        property real pressedX: 0
        property real pressedY: 0
        property bool dragging: false

        onPressed: mouse => {
            pressedX = mouse.x
            pressedY = mouse.y
            dragging = false
        }
        onPositionChanged: mouse => {
            if (!tile.draggable || !(pressedButtons & Qt.LeftButton)) {
                return
            }
            if (!dragging && Math.hypot(mouse.x - pressedX, mouse.y - pressedY) < dragStartDistance) {
                return
            }
            dragging = true
            const point = mapToItem(null, mouse.x, mouse.y)
            tile.dragMoved(point.x, point.y)
        }
        onCanceled: dragging = false
        onClicked: mouse => {
            if (dragging) {
                dragging = false
                return
            }
            if (mouse.button === Qt.RightButton) {
                tile.menuWanted()
            } else {
                tile.activate()
            }
        }
    }
}
