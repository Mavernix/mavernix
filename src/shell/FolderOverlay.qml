// A clear full-screen window above everything that shows an open folder's
// grid or fan. A press outside it closes it.

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
            grid.scrollToTop()
        }
    }
    Behavior on shown {
        NumberAnimation { duration: overlay.open ? overlay.openDuration : overlay.closeDuration }
    }

    // The window stays until the going-away motion is over.
    visible: open || shown > 0

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    // Cover panels and docks too, so that positions in this window are
    // positions on the screen.
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    color: "transparent"

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: overlay.dismissed()
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
        // Clear of the screen's sides and of the panel's height at the top.
        mostWidth: overlay.width - 2 * sideRoom
        mostHeight: tipY - pointerLength - sideRoom - (overlay.settings.theme.panel?.height ?? 0)
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
