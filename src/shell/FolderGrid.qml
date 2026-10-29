// A folder's contents as a grid of icons in a box pointing at its dock
// tile, with a last entry that opens the folder. Looks: the theme's
// [dock.folder.grid] (docs/themes/, "Folders in the dock"); without box
// pictures the box is drawn like a menu's.
//
// How big it gets is FolderGridLayout.js's; one entry is a FolderGridCell.
// A folder with more than fits scrolls, by wheel or drag, and comes to rest
// on a whole row. The arrow keys choose an entry, Return opens it, and
// typing jumps to a name (handleKey, called by FolderOverlay). An entry with
// `entries` of its own is a folder inside: it opens in the same box, with a
// back button.

import QtQuick
import QtQuick.Effects
import "FolderGridLayout.js" as FolderGridLayout
import "ThemeValues.js" as ThemeValues

Item {
    id: grid

    IconSource {
        id: themeIcons
    }

    required property var settings
    // Menu-like box looks, for a theme without pictures for this box.
    required property var plainBoxLooks

    property string title: ""
    // { name, path, icon, genericIcon } each; `entries` too for a folder inside.
    property var entries: []
    property string openLabel: ""
    // The pointer tip's distance from the box's left edge.
    property real pointerX: -1
    // The free part of the screen: its width, and its height between the
    // panel and the dock.
    required property real freeWidth
    required property real freeHeight

    signal fileChosen(string path)
    signal folderChosen()

    readonly property var looks: settings.theme.dock.folder?.grid ?? ({})
    readonly property var font: settings.theme.fonts.dock_folder ?? settings.theme.fonts.interface
    readonly property string themeDirectory: settings.theme_directory

    readonly property real cellWidth: looks.cell_width ?? 112
    readonly property real cellHeight: looks.cell_height ?? 112
    readonly property real sidePadding: looks.side_padding ?? 12
    readonly property real firstRowTop: looks.first_row_top ?? 36
    readonly property real iconSize: looks.icon_size ?? 64
    readonly property real bottomPadding: looks.bottom_padding ?? 8

    // --- Folders inside -----------------------------------------------------------

    // The folders inside that were opened, the innermost last, each with
    // where the view stood when it was opened, for going back:
    // [{ folder, contentY, selected }].
    property var trail: []
    readonly property var shownEntries: trail.length > 0 ? trail[trail.length - 1].folder.entries : entries
    readonly property string shownTitle: trail.length > 0 ? trail[trail.length - 1].folder.name : title

    // Back to the start, as when the folder opens.
    function reset() {
        trail = []
        cells.contentY = 0
        selectedIndex = -1
        typed = ""
    }

    function openInside(folder) {
        trail = trail.concat([{ folder: folder, contentY: cells.contentY, selected: selectedIndex }])
        snap.stop()
        cells.contentY = 0
        selectedIndex = -1
        typed = ""
    }

    // Back to the folder before, where it stood.
    function goBack() {
        if (trail.length === 0) {
            return
        }
        const left = trail[trail.length - 1]
        trail = trail.slice(0, -1)
        snap.stop()
        cells.contentY = left.contentY
        selectedIndex = left.selected
    }

    function open(index) {
        const file = index < shownEntries.length ? shownEntries[index] : null
        if (file === null) {
            folderChosen()
        } else if (file.entries !== undefined) {
            openInside(file)
        } else {
            fileChosen(file.path)
        }
    }

    // --- Size ---------------------------------------------------------------------

    readonly property int entryCount: shownEntries.length + 1
    readonly property var layout: FolderGridLayout.layout(looks, entryCount, freeWidth, freeHeight)
    readonly property int columns: layout.columns
    readonly property int shownRows: layout.shownRows
    readonly property int rows: layout.rows
    readonly property real cellScale: layout.cellScale
    readonly property real shownCellWidth: cellWidth * cellScale
    readonly property real shownCellHeight: cellHeight * cellScale
    readonly property real shownIconSize: iconSize * cellScale

    width: 2 * sidePadding + columns * shownCellWidth
    height: firstRowTop + shownRows * shownCellHeight + bottomPadding

    // --- The entry the keys chose ---------------------------------------------

    // -1 while there is none.
    property int selectedIndex: -1
    // What was typed to find an entry by its name.
    property string typed: ""
    // Letters typed after this long a pause start a new name.
    readonly property int typingPauseMs: 1000

    Timer {
        id: typingPause
        interval: grid.typingPauseMs
        onTriggered: grid.typed = ""
    }

    function select(index) {
        selectedIndex = Math.max(0, Math.min(entryCount - 1, index))
        showRowOf(selectedIndex)
    }

    // The first press chooses the first entry; later ones move from the chosen one.
    function moveSelection(columnsBy, rowsBy) {
        if (selectedIndex < 0) {
            select(0)
            return
        }
        select(selectedIndex + columnsBy + rowsBy * columns)
    }

    function typeToSelect(letter) {
        typed += letter.toLowerCase()
        typingPause.restart()
        const found = shownEntries.findIndex(file => file.name.toLowerCase().startsWith(typed))
        if (found >= 0) {
            select(found)
        }
    }

    // Whether the key was one the grid uses.
    function handleKey(event) {
        if (event.key === Qt.Key_Left) {
            moveSelection(-1, 0)
        } else if (event.key === Qt.Key_Right) {
            moveSelection(1, 0)
        } else if (event.key === Qt.Key_Up) {
            moveSelection(0, -1)
        } else if (event.key === Qt.Key_Down) {
            moveSelection(0, 1)
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && selectedIndex >= 0) {
            open(selectedIndex)
        } else if (event.key === Qt.Key_Backspace && trail.length > 0) {
            goBack()
        } else if (event.text.length === 1 && event.text.trim() !== "") {
            typeToSelect(event.text)
        } else {
            return false
        }
        return true
    }

    // --- Scrolling ----------------------------------------------------------------

    // Moves the view to `goal`, as a scroll comes to rest.
    function scrollTo(goal) {
        snap.stop()
        snap.to = goal
        snap.start()
    }

    // Scrolls the least that brings the entry's row into view.
    function showRowOf(index) {
        const top = Math.floor(index / columns) * shownCellHeight
        if (top < cells.contentY) {
            scrollTo(top)
        } else if (top + shownCellHeight > cells.contentY + cells.height) {
            scrollTo(top + shownCellHeight - cells.height)
        }
    }

    // Comes to rest on the nearest whole row.
    function snapToRow() {
        const lowest = Math.max(0, cells.contentHeight - cells.height)
        const goal = Math.max(0, Math.min(lowest, Math.round(cells.contentY / shownCellHeight) * shownCellHeight))
        if (Math.abs(goal - cells.contentY) >= 0.5) {
            scrollTo(goal)
        }
    }

    NumberAnimation {
        id: snap
        target: cells
        property: "contentY"
        duration: grid.looks.scroll_snap_ms ?? 0
        easing.type: ThemeValues.easing(grid.looks.scroll_snap_curve ?? "ease-out")
    }

    // How far a grid that scrolls fades out at its top and bottom.
    readonly property real fadeLength: cells.interactive ? (looks.scroll_fade ?? 0) : 0

    // --- What is drawn --------------------------------------------------------------

    // Presses on the box that hit no entry stay with it.
    MouseArea {
        anchors.fill: parent
    }

    Loader {
        anchors.fill: parent
        sourceComponent: grid.looks.box !== undefined ? picturedBox : plainBox
    }
    Component {
        id: picturedBox
        PiecedBox {
            looks: grid.looks.box
            themeDirectory: grid.themeDirectory
            pointerX: grid.pointerX
        }
    }
    Component {
        id: plainBox
        BoxFrame {
            looks: grid.plainBoxLooks
            pointerX: grid.pointerX
        }
    }

    FolderGridText {
        folderGrid: grid
        text: grid.shownTitle
        color: ThemeValues.color(grid.looks.title_color ?? grid.looks.label_color ?? "#ffffff")
        anchors.horizontalCenter: parent.horizontalCenter
        // Placed by its baseline.
        y: (grid.looks.title_baseline ?? 18) - baselineOffset
    }

    FolderBackButton {
        folderGrid: grid
        visible: grid.trail.length > 0
        onClicked: grid.goBack()
    }

    Flickable {
        id: cells
        y: grid.firstRowTop
        width: grid.width
        height: grid.shownRows * grid.shownCellHeight
        contentHeight: grid.rows * grid.shownCellHeight
        interactive: grid.rows > grid.shownRows
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        onMovementEnded: grid.snapToRow()

        // The fade at the top and bottom: the cells drawn through fadeMask.
        layer.enabled: grid.fadeLength > 0
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: fadeMask
            // A soft mask: its alpha, not just above or below a threshold.
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }

        Repeater {
            model: grid.entryCount

            FolderGridCell {
                folderGrid: grid
                icons: themeIcons
                file: index < grid.shownEntries.length ? grid.shownEntries[index] : null
                x: grid.sidePadding + (index % grid.columns) * grid.shownCellWidth
                y: Math.floor(index / grid.columns) * grid.shownCellHeight
                width: grid.shownCellWidth
                height: grid.shownCellHeight
            }
        }
    }

    EdgeFadeMask {
        id: fadeMask
        width: cells.width
        height: cells.height
        fadeLength: grid.fadeLength
    }

    FolderScrollBar {
        folderGrid: grid
        view: cells
        settling: snap.running
    }
}
