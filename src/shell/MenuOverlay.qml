// The full-screen, transparent window open menus live in; a press outside
// the menu closes it. One rectangle (`passX` and so on, the panel) is left
// out, so the panel's titles can still switch menus while one is open.
//
// A menu opens at once and goes away as [animation.menu_close] says: a
// chosen item blinks, then the menu fades. Whoever sets `open` keeps the
// menu's place until it has faded (`visible` turns false then); the items
// it shows stay as they were when it closed, whatever the choice changes.
// While it fades, the pointer and keys already reach what is underneath.

import QtQuick
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: overlay

    required property var settings

    property bool open: false

    property var items: []
    // What the box shows: the items, held still once the menu closes.
    property var shownItems: []
    onItemsChanged: {
        if (open) {
            shownItems = items
        }
    }
    property real menuX: 0
    property real menuY: 0
    // Whether the menu hangs from something along its whole top edge.
    property bool squareTop: false
    // `looks` overrides the theme's [menu]; `pointerScreenX` places a pointer
    // on the bottom edge, in screen coordinates (negative: none).
    property var looks: settings.theme.menu
    property real pointerScreenX: -1
    // The most height the menu may take (0: no limit; a longer one scrolls),
    // and whether a menu that scrolls opens at its end.
    property real mostMenuHeight: 0
    property bool startsAtEnd: false

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

    // --- Going away ---------------------------------------------------------------

    // { kind: "fade" | "none", duration_ms, curve, blink_ms }
    readonly property var closeMotion: (settings.theme.animation ?? ({})).menu_close ?? ({ kind: "none" })
    readonly property bool fades: closeMotion.kind === "fade"

    // Set between a choice and the menu's closing, so the closing blinks.
    property bool justChosen: false
    // The menu whose chosen item is blinking.
    property var blinkingMenu: null

    visible: open || goingAway.running

    onOpenChanged: {
        goingAway.stop()
        if (open) {
            shownItems = items
            box.opacity = 1
            keys.forceActiveFocus()
        } else if (fades) {
            if (justChosen && (closeMotion.blink_ms ?? 0) > 0) {
                blinkingMenu = box.innermost()
                blinkingMenu.blinking = true
            }
            goingAway.start()
        }
        justChosen = false
    }

    // --- A press that drags into the menu ------------------------------------------
    //
    // A press on what opened the menu (a panel title, a dock icon held down)
    // keeps the pointer until it is released, so its owner passes the drag
    // on, as points in this window: the item under the pointer is marked,
    // and on release chosen, as on a Mac. Not once the menu has closed.

    function markAt(x, y) {
        if (!open) {
            return
        }
        const hit = box.rowAt(x - box.x, y - box.y)
        if (hit !== null && hit.index >= 0 && hit.menu.canBeMarked(hit.index)) {
            hit.menu.markRow(hit.index)
            return
        }
        // Off the items: nothing is marked, as nothing would be chosen.
        const menu = hit?.menu ?? box.innermost()
        menu.hoveredIndex = -1
        menu.waitingIndex = -1
    }

    // Whether there was an item to choose.
    function chooseAt(x, y) {
        if (!open) {
            return false
        }
        const hit = box.rowAt(x - box.x, y - box.y)
        if (hit === null || hit.index < 0 || !hit.menu.canBeMarked(hit.index)) {
            return false
        }
        hit.menu.markRow(hit.index)
        hit.menu.chooseMarked()
        return true
    }

    SequentialAnimation {
        id: goingAway
        // Without a blinking menu the pause is skipped.
        PauseAnimation {
            duration: overlay.blinkingMenu !== null ? overlay.closeMotion.blink_ms : 0
        }
        ScriptAction {
            script: overlay.stopBlinking()
        }
        NumberAnimation {
            target: box
            property: "opacity"
            to: 0
            duration: overlay.closeMotion.duration_ms ?? 150
            easing.type: ThemeValues.easing(overlay.closeMotion.curve ?? "linear")
        }
        // Stopped early, by opening again or by going away, it must not
        // leave a blink behind.
        onStopped: overlay.stopBlinking()
    }

    function stopBlinking() {
        if (blinkingMenu !== null) {
            blinkingMenu.blinking = false
            blinkingMenu = null
        }
    }

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    // Cover panels too, so that window positions are screen positions.
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: open ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    // Nothing while the menu fades, so a press goes straight through.
    mask: Region {
        item: overlay.open ? everything : null

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
    MenuBox {
        id: box
        settings: overlay.settings
        looks: overlay.looks
        pointerX: overlay.pointerScreenX >= 0 ? overlay.pointerScreenX - x : -1
        items: overlay.shownItems
        x: Math.max(0, Math.min(overlay.menuX, overlay.width - width))
        y: overlay.menuY
        still: !overlay.open
        squareTopLeft: overlay.squareTop
        squareTopRight: overlay.squareTop
        mostHeight: overlay.mostMenuHeight
        startsAtEnd: overlay.startsAtEnd
        onChosen: id => {
            overlay.justChosen = true
            overlay.chosen(id)
            // Not closed by the choice after all (a checkable item, say).
            overlay.justChosen = false
        }
        onSubmenuWanted: id => overlay.submenuWanted(id)
        onSlid: (id, value) => overlay.slid(id, value)
    }
}
