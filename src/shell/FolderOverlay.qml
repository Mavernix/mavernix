// A clear full-screen window above everything that shows an open folder's
// grid or fan. A press outside it, or Escape, closes it; a grid takes the
// other keys (FolderGrid.handleKey).

import QtQuick
import Quickshell
import Quickshell.Wayland

PanelWindow {
    id: overlay

    required property var settings
    required property var plainBoxLooks

    property bool open: false
    // "grid" or "fan".
    property string view: "grid"
    property string title: ""
    property var entries: []
    property string openLabel: ""
    property string moreLabel: "%1"

    // The middle of the folder tile's top edge, on the screen.
    property real anchorX: 0
    property real anchorY: 0
    // How much of the screen's height the dock takes while resting.
    property real dockThickness: 0

    signal fileChosen(string path)
    signal folderChosen()
    signal dismissed()

    // [animation.dock_folder] for a grid, { kind: "none" | "fade" | "grow",
    // duration_ms, close_duration_ms }; [animation.dock_folder_fan] for a
    // fan, { kind: "none" | "rise", ... }.
    readonly property var motions: settings.theme.animation ?? ({})
    readonly property var motion:
        (view === "fan" ? motions.dock_folder_fan : motions.dock_folder) ?? ({ kind: "none" })
    readonly property int openDuration: motion.kind === "none" ? 0 : (motion.duration_ms ?? 150)
    readonly property int closeDuration: motion.kind === "none" ? 0
                                         : (motion.close_duration_ms ?? openDuration)
    property real shown: open ? 1 : 0
    onOpenChanged: {
        if (open) {
            grid.reset()
        } else {
            warmUp()
        }
    }
    Behavior on shown {
        NumberAnimation { duration: overlay.open ? overlay.openDuration : overlay.closeDuration }
    }

    // The window stays until the going-away motion is over, and then for a
    // minute, empty and letting the pointer through: made anew, it took
    // most of the time a folder needed to open (on an NVIDIA card, a grid's
    // first frame came 110 to 180 ms after the click, against 50 to 90 ms
    // with the window already made). warmUp() makes it ahead of time, when
    // the pointer comes to rest on a folder.
    readonly property bool showing: open || shown > 0
    visible: showing || keptMade
    // Not the timer's `running`: restarting a running timer stops it first,
    // which took the window down and made it anew at every pass of the
    // pointer.
    property bool keptMade: false
    function warmUp() {
        keptMade = true
        staysMade.restart()
    }
    Timer {
        id: staysMade
        interval: 60000
        onTriggered: overlay.keptMade = false
    }
    mask: Region {
        item: overlay.showing ? pressesOutside : null
    }

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    // Cover panels and docks too, so that positions in this window are
    // positions on the screen.
    exclusionMode: ExclusionMode.Ignore
    // Waiting empty, on the top layer, which the compositor leaves out under
    // a fullscreen window (a film, a game) instead of drawing it over every
    // frame.
    WlrLayershell.layer: showing ? WlrLayer.Overlay : WlrLayer.Top
    // The keys, while open, as a menu takes them.
    WlrLayershell.keyboardFocus: open ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    MouseArea {
        id: pressesOutside
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: overlay.dismissed()
    }

    Item {
        focus: overlay.open
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) {
                overlay.dismissed()
            } else if (overlay.view !== "grid" || !grid.handleKey(event)) {
                return
            }
            event.accepted = true
        }
    }

    FolderFan {
        visible: overlay.view === "fan"
        settings: overlay.settings
        entries: overlay.entries
        openLabel: overlay.openLabel
        moreLabel: overlay.moreLabel
        x: overlay.anchorX
        y: overlay.anchorY
        shown: overlay.shown

        onFileChosen: path => overlay.fileChosen(path)
        onFolderChosen: overlay.folderChosen()
    }

    FolderGrid {
        id: grid
        visible: overlay.view === "grid"
        settings: overlay.settings
        plainBoxLooks: overlay.plainBoxLooks
        title: overlay.title
        entries: overlay.entries
        openLabel: overlay.openLabel

        readonly property var boxLooks: looks.box ?? overlay.plainBoxLooks
        readonly property real pointerLength: boxLooks.pointer_height ?? boxLooks.pointer?.height ?? 0
        // Centred on the tip but kept off the screen's sides.
        readonly property real sideRoom: looks.screen_margin ?? 16
        readonly property real tipY: overlay.anchorY - (boxLooks.tile_gap ?? overlay.plainBoxLooks.tile_gap ?? 4)
        x: Math.round(Math.max(sideRoom, Math.min(overlay.width - sideRoom - width,
                                                  overlay.anchorX - width / 2)))
        y: Math.round(tipY - pointerLength - height)
        freeWidth: overlay.width
        freeHeight: overlay.height - overlay.dockThickness - (overlay.settings.theme.panel?.height ?? 0)
        pointerX: overlay.anchorX - x

        opacity: overlay.shown
        transform: Scale {
            origin.x: grid.pointerX
            origin.y: grid.height + grid.pointerLength
            xScale: overlay.motion.kind === "grow" ? overlay.shown : 1
            yScale: xScale
        }

        onFileChosen: path => overlay.fileChosen(path)
        onFolderChosen: overlay.folderChosen()
    }
}
