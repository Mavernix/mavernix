// The full-screen, transparent window open menus live in; a press outside
// the menu closes it. One rectangle (`passX` and so on, the panel) is left
// out, so the panel's titles can still switch menus while one is open.

import QtQuick
import Quickshell
import Quickshell.Wayland

PanelWindow {
    id: overlay

    required property var settings

    property var items: []
    property real menuX: 0
    property real menuY: 0
    // Whether the menu hangs from something along its whole top edge.
    property bool squareTop: false
    // `looks` overrides the theme's [menu]; `pointerScreenX` places a pointer
    // on the bottom edge, in screen coordinates (negative: none).
    property var looks: settings.theme.menu
    property real pointerScreenX: -1

    property real passX: 0
    property real passY: 0
    property real passWidth: 0
    property real passHeight: 0

    readonly property real menuHeight: box.height
    readonly property real menuWidth: box.width

    signal chosen(int id)
    signal submenuWanted(int id)
    signal slid(int id, real value)
    signal dismissed()
    // Arrow keys past the menu's ends ask for its neighbour (-1 or 1).
    signal neighbourWanted(int direction)

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    // Cover panels too, so that window positions are screen positions.
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: visible ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    mask: Region {
        item: everything

        Region {
            x: overlay.passX
            y: overlay.passY
            width: overlay.passWidth
            height: overlay.passHeight
            intersection: Intersection.Subtract
        }
    }

    MouseArea {
        id: everything
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: overlay.dismissed()
    }

    Item {
        id: keys
        focus: true
        Keys.onPressed: event => {
            const menu = box.innermost()
            if (event.key === Qt.Key_Escape) {
                overlay.dismissed()
            } else if (event.key === Qt.Key_Down) {
                menu.moveMark(1)
            } else if (event.key === Qt.Key_Up) {
                menu.moveMark(-1)
            } else if (event.key === Qt.Key_Right) {
                if (!menu.openMarkedSubmenu()) {
                    overlay.neighbourWanted(1)
                }
            } else if (event.key === Qt.Key_Left) {
                if (!menu.closeAsSubmenu()) {
                    overlay.neighbourWanted(-1)
                }
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                       || event.key === Qt.Key_Space) {
                menu.chooseMarked()
            } else if (event.text !== "" && event.text.trim() !== "" && (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0) {
                // Letters typed in quick succession mark the next matching item.
                keys.typed += event.text
                typedSoFar.restart()
                menu.markByName(keys.typed)
            } else {
                return
            }
            event.accepted = true
        }

        property string typed: ""
        Timer {
            id: typedSoFar
            interval: 1000
            onTriggered: keys.typed = ""
        }
    }
    onVisibleChanged: {
        if (visible) {
            keys.forceActiveFocus()
        }
    }

    MenuBox {
        id: box
        settings: overlay.settings
        looks: overlay.looks
        pointerX: overlay.pointerScreenX >= 0 ? overlay.pointerScreenX - x : -1
        items: overlay.items
        x: Math.max(0, Math.min(overlay.menuX, overlay.width - width))
        y: overlay.menuY
        squareTopLeft: overlay.squareTop
        squareTopRight: overlay.squareTop
        onChosen: id => overlay.chosen(id)
        onSubmenuWanted: id => overlay.submenuWanted(id)
        onSlid: (id, value) => overlay.slid(id, value)
    }
}
