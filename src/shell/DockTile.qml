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
    // An icon that is not the icon theme's own (0 to 1, 0 for any size) spans
    // at most this share of its square, as the theme's leave a margin.
    property real foreignIconFill: 0
    // For a running program that has no desktop entry: its windows' app ID.
    property string appId: ""
    property var minimizedWindow: null
    // For a tile that is neither an application nor a window.
    property string picture: ""
    property bool pictureComing: false
    property string name: ""
    property var command: []
    // Something for the shell to do on a press, e.g. "app_grid".
    property string shellAction: ""
    signal shellActionWanted(string action)
    property bool opensFolder: false
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
        const name = entry !== null ? themeIcons.nameFor(entry) : appId
        if (entry === null && appId === "") {
            return ""
        }
        return themeIcons.fitted(name, foreignIconFill, "application-x-executable")
    }
    readonly property real iconSize: width - 2 * margin
    readonly property real iconX: x + margin

    readonly property var windows: {
        if (entry !== null) {
            return ToplevelManager.toplevels.values.filter(
                window => DesktopEntries.heuristicLookup(window.appId) === tile.entry)
        }
        if (appId !== "") {
            return ToplevelManager.toplevels.values.filter(window => window.appId === tile.appId)
        }
        return []
    }
    readonly property bool running: minimizedWindow === null && windows.length > 0

    // In pixels; the dock moves the tile up by this and the reflection down.
    property real lift: 0

    // Until the first window appears or bounce.longest_ms has passed.
    property bool launching: false
    // Set by the dock while a window of this program asks for attention:
    // the icon bounces as for a launch, as a Mac's Dock does, until the
    // program no longer asks.
    property bool wantsAttention: false
    onWantsAttentionChanged: {
        if (wantsAttention && !bounceAnimation.running) {
            bounceAnimation.start()
        }
    }
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

    // One bounce is a parabola, repeated while launching or asking for
    // attention, and always finished.
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
            if (tile.launching || tile.wantsAttention) {
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
        if (minimizedWindow !== null) {
            return minimizedWindow.title
        }
        return entry?.name ?? appId
    }
    // Set by the dock, which tracks the pointer across all tiles for
    // magnification; a hover handler here would hide it from the dock.
    property bool pointerOver: false
    readonly property bool hovered: pointerOver && press.enabled

    signal menuWanted()
    // A press held this long opens the menu too, as on a Mac (on 10.9, about
    // half a second).
    readonly property int holdForMenuMs: 500
    // A press held until the menu opened may drag on to an item and let go
    // to choose it. These report where, in window coordinates.
    signal menuDragged(real windowX, real windowY)
    signal menuReleased(real windowX, real windowY)

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
        if (opensFolder) {
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
        if (entry === null) {
            return
        }
        entry.execute()
        startBouncing()
    }

    // Runs one of the program's own entries (a desktop entry action, such as
    // "New Window"). One that starts the program bounces like a launch.
    function runAction(action) {
        action.execute()
        if (!running) {
            startBouncing()
        }
    }

    function startBouncing() {
        launching = true
        bounceAnimation.start()
    }

    MouseArea {
        id: press
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        enabled: tile.entry !== null || tile.appId !== "" || tile.minimizedWindow !== null
                 || tile.command.length > 0
                 || tile.shellAction !== "" || tile.opensFolder

        // In pixels.
        readonly property real dragStartDistance: 8
        property real pressedX: 0
        property real pressedY: 0
        property bool dragging: false
        // The press was held until the menu opened.
        property bool heldForMenu: false

        pressAndHoldInterval: tile.holdForMenuMs
        onPressed: mouse => {
            pressedX = mouse.x
            pressedY = mouse.y
            dragging = false
            heldForMenu = false
        }
        onPressAndHold: mouse => {
            if (dragging || mouse.button !== Qt.LeftButton) {
                // Not taken, so the release still clicks.
                mouse.accepted = false
                return
            }
            heldForMenu = true
            tile.menuWanted()
        }
        onReleased: mouse => {
            if (heldForMenu) {
                const point = mapToItem(null, mouse.x, mouse.y)
                tile.menuReleased(point.x, point.y)
            }
        }
        onPositionChanged: mouse => {
            if (heldForMenu) {
                const point = mapToItem(null, mouse.x, mouse.y)
                tile.menuDragged(point.x, point.y)
                return
            }
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
