// A folder's contents as a grid of icons in a box pointing at its dock
// tile, with a last entry that opens the folder. A folder with more than
// fits scrolls, by wheel or drag, and comes to rest on a whole row. The
// arrow keys choose an entry, Return opens it, and typing jumps to a name
// (handleKey, called by FolderOverlay). An entry with `entries` of its own is
// a folder inside: it opens in the same box, with a back button. A name too
// long for its cell shows whole while the pointer is on it. Looks: the theme's
// [dock.folder.grid] (docs/themes/, "Folders in the dock"); without box
// pictures the box is drawn like a menu's.

import QtQuick
import QtQuick.Effects
import Quickshell
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
    // The folders inside that were opened, the innermost last, each with
    // where the view stood when it was opened, for going back:
    // [{ folder, contentY, selected }].
    property var trail: []
    readonly property var shownEntries: trail.length > 0 ? trail[trail.length - 1].folder.entries : entries
    readonly property string shownTitle: trail.length > 0 ? trail[trail.length - 1].folder.name : title
    // { name, path, icon, genericIcon } each, from FolderContents.
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

    // --- How big the grid is -----------------------------------------------
    // The grid is sized for a stage: the share `stage_share` of the free
    // width and height. As many cells as the stage holds are shown at once,
    // each counted with `fit_spacing` more room than it takes; the rest
    // scroll. The shown cells make a near square, and shrink if the stage is
    // too small for them at full size (docs/themes/dock.md, "Folders in the
    // dock").
    readonly property real stageShare: looks.stage_share ?? 1
    readonly property real fitSpacing: looks.fit_spacing ?? 0
    readonly property real stageWidth: Math.floor(Math.min(freeWidth * stageShare, freeWidth - 2 * sidePadding))
    readonly property real stageHeight:
        Math.floor(Math.min(freeHeight * stageShare, freeHeight - firstRowTop - bottomPadding))

    readonly property int entryCount: shownEntries.length + 1
    readonly property int cellsThatFit: Math.max(1, Math.floor(
        stageWidth / (cellWidth + fitSpacing) * (stageHeight / (cellHeight + fitSpacing))))
    readonly property int shownCells: Math.min(entryCount, cellsThatFit)

    // Columns and rows for `count` cells: as many columns as its square root,
    // and one more when the last row would otherwise hold no more cells than
    // one fewer than there are rows (so 3 cells make 3 x 1, 7 make 4 x 2).
    function nearSquare(count) {
        let columns = Math.ceil(Math.sqrt(count))
        let rows = Math.ceil(count / columns)
        const inLastRow = count % columns
        if (inLastRow !== 0 && inLastRow <= rows - 1) {
            columns += 1
            rows = Math.ceil(count / columns)
        }
        return { columns: columns, rows: rows }
    }

    readonly property var shownShape: nearSquare(shownCells)
    readonly property int columns: shownShape.columns
    readonly property int shownRows: shownShape.rows
    readonly property int rows: Math.ceil(entryCount / columns)

    // Cells shrink to fit the stage, in whole pixels, but never grow.
    readonly property real cellScale: Math.min(1, Math.floor(stageWidth / columns) / cellWidth,
                                               Math.floor(stageHeight / shownRows) / cellHeight)
    readonly property real shownCellWidth: cellWidth * cellScale
    readonly property real shownCellHeight: cellHeight * cellScale
    readonly property real shownIconSize: iconSize * cellScale

    width: 2 * sidePadding + columns * shownCellWidth
    height: firstRowTop + shownRows * shownCellHeight + bottomPadding

    function scrollToTop() {
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

    // --- The entry the keys chose ------------------------------------------

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

    // Scrolls the least that brings the entry's row into view, moving as a
    // scroll comes to rest.
    function showRowOf(index) {
        const top = Math.floor(index / columns) * shownCellHeight
        let goal = -1
        if (top < cells.contentY) {
            goal = top
        } else if (top + shownCellHeight > cells.contentY + cells.height) {
            goal = top + shownCellHeight - cells.height
        }
        if (goal >= 0) {
            snap.stop()
            snap.to = goal
            snap.start()
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

    // --- Scrolling -------------------------------------------------------------

    // How far a grid that scrolls fades out at its top and bottom.
    readonly property real fadeLength: cells.interactive ? (looks.scroll_fade ?? 0) : 0

    // Comes to rest on the nearest whole row.
    function snapToRow() {
        const lowest = Math.max(0, cells.contentHeight - cells.height)
        const goal = Math.max(0, Math.min(lowest, Math.round(cells.contentY / shownCellHeight) * shownCellHeight))
        if (Math.abs(goal - cells.contentY) < 0.5) {
            return
        }
        snap.to = goal
        snap.start()
    }

    NumberAnimation {
        id: snap
        target: cells
        property: "contentY"
        duration: grid.looks.scroll_snap_ms ?? 0
        easing.type: ThemeValues.easing(grid.looks.scroll_snap_curve ?? "ease-out")
    }

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

    component GridText: Text {
        color: ThemeValues.color(grid.looks.label_color ?? "#ffffff")
        font.family: grid.font.family
        font.pixelSize: grid.font.size
        font.bold: grid.font.bold ?? false
    }

    GridText {
        text: grid.shownTitle
        color: ThemeValues.color(grid.looks.title_color ?? grid.looks.label_color ?? "#ffffff")
        anchors.horizontalCenter: parent.horizontalCenter
        // Placed by its baseline.
        y: (grid.looks.title_baseline ?? 18) - baselineOffset
    }

    // Back to the folder before: the theme's picture, else a plain arrow.
    readonly property var backLooks: looks.back_button ?? ({})
    Item {
        id: backButton
        visible: grid.trail.length > 0
        x: grid.backLooks.left ?? grid.sidePadding
        y: grid.backLooks.top ?? 4
        width: backPicture.visible ? backPicture.implicitWidth : backArrow.implicitWidth + 8
        height: backPicture.visible ? backPicture.implicitHeight : backArrow.implicitHeight
        Image {
            id: backPicture
            visible: grid.backLooks.image !== undefined
            source: !visible ? "" : "file://" + grid.themeDirectory + "/"
                + (backPress.pressed ? (grid.backLooks.pressed_image ?? grid.backLooks.image) : grid.backLooks.image)
        }
        GridText {
            id: backArrow
            visible: !backPicture.visible
            anchors.centerIn: parent
            text: "‹"
            // Bigger than the names, so it reads as a button.
            font.pixelSize: Math.round(grid.font.size * 2)
            color: ThemeValues.color(grid.looks.label_color ?? "#ffffff")
            opacity: backPress.pressed ? 0.6 : 1
        }
        MouseArea {
            id: backPress
            anchors.fill: parent
            onClicked: grid.goBack()
        }
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

            Item {
                id: cell
                required property int index
                readonly property var file: index < grid.shownEntries.length ? grid.shownEntries[index] : null
                // Above its neighbours while its whole name shows.
                z: wholeName.visible ? 1 : 0

                x: grid.sidePadding + (index % grid.columns) * grid.shownCellWidth
                y: Math.floor(index / grid.columns) * grid.shownCellHeight
                width: grid.shownCellWidth
                height: grid.shownCellHeight

                Fill {
                    anchors.fill: parent
                    visible: cell.index === grid.selectedIndex && grid.looks.selection !== undefined
                    fill: grid.looks.selection ?? ({})
                    themeDirectory: grid.themeDirectory
                }

                readonly property bool hasOwnPicture: file === null && grid.looks.open_image !== undefined
                readonly property real pictureSize: hasOwnPicture
                    ? (grid.looks.open_image_size ?? grid.iconSize) * grid.cellScale
                    : grid.shownIconSize
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: (grid.shownIconSize - cell.pictureSize) / 2
                    width: cell.pictureSize
                    height: cell.pictureSize
                    sourceSize: Qt.size(width, height)
                    source: {
                        if (cell.hasOwnPicture) {
                            return "file://" + grid.themeDirectory + "/" + grid.looks.open_image
                        }
                        if (cell.file === null) {
                            return themeIcons.path("folder-open")
                        }
                        if (cell.file.entries !== undefined) {
                            return themeIcons.path(cell.file.icon || "folder", "folder")
                        }
                        // The file's own kind, else its family, else a plain document.
                        const own = themeIcons.find(cell.file.icon)
                        return own !== "" ? own
                                          : themeIcons.path(cell.file.genericIcon, "text-x-generic")
                    }
                    opacity: press.pressed ? 0.6 : 1
                }

                GridText {
                    id: name
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(implicitWidth,
                                    (grid.looks.label_width ?? (grid.cellWidth - 8)) * grid.cellScale)
                    elide: Text.ElideMiddle
                    y: (grid.looks.label_baseline ?? (grid.iconSize + 16)) * grid.cellScale - baselineOffset
                    text: cell.file !== null ? cell.file.name : grid.openLabel
                    style: grid.looks.label_shadow_color !== undefined ? Text.Raised : Text.Normal
                    styleColor: ThemeValues.color(grid.looks.label_shadow_color ?? "#00000000")
                }

                // The whole name, on the theme's label_expanded, while the
                // pointer is on an entry whose name is cut short.
                readonly property var expandedLooks: grid.looks.label_expanded ?? null
                Item {
                    id: wholeName
                    visible: press.containsMouse && name.truncated && cell.expandedLooks !== null
                    readonly property real padding: cell.expandedLooks?.padding ?? 8
                    width: wholeText.implicitWidth + 2 * padding
                    height: cell.expandedLooks?.height ?? (wholeText.implicitHeight + 4)
                    // Centred on the entry, but moved in rather than cut off
                    // at the box's sides.
                    x: Math.max(grid.sidePadding / 2 - cell.x,
                                Math.min(grid.width - grid.sidePadding / 2 - width - cell.x,
                                         (cell.width - width) / 2))
                    y: Math.round(name.y + name.height / 2 - height / 2)
                    Fill {
                        anchors.fill: parent
                        fill: cell.expandedLooks?.fill ?? ({})
                        themeDirectory: grid.themeDirectory
                    }
                    GridText {
                        id: wholeText
                        anchors.centerIn: parent
                        text: name.text
                        color: ThemeValues.color(cell.expandedLooks?.text_color ?? "#000000")
                    }
                }

                MouseArea {
                    id: press
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: grid.open(cell.index)
                }
            }
        }
    }

    // White where the cells show, clear where they fade out (see cells' layer).
    Item {
        id: fadeMask
        visible: false
        layer.enabled: true
        width: cells.width
        height: cells.height

        Rectangle {
            width: parent.width
            height: grid.fadeLength
            gradient: Gradient {
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 1; color: "white" }
            }
        }
        Rectangle {
            y: grid.fadeLength
            width: parent.width
            height: parent.height - 2 * grid.fadeLength
            color: "white"
        }
        Rectangle {
            y: parent.height - grid.fadeLength
            width: parent.width
            height: grid.fadeLength
            gradient: Gradient {
                GradientStop { position: 0; color: "white" }
                GradientStop { position: 1; color: "transparent" }
            }
        }
    }

    // Shows where the view stands while it moves, for a theme that gives it
    // a width: the theme's picture stretched along its middle
    // (scroll_bar_image, scroll_bar_slice), else a bar in a colour. It runs
    // along the cells, or along the box less scroll_bar_inset at each end.
    Item {
        id: scrollBar
        visible: cells.interactive && thickness > 0
        readonly property real thickness: grid.looks.scroll_bar_width ?? 0
        readonly property bool alongBox: grid.looks.scroll_bar_inset !== undefined
        readonly property real trackTop: alongBox ? grid.looks.scroll_bar_inset : cells.y
        readonly property real trackLength:
            alongBox ? grid.height - 2 * grid.looks.scroll_bar_inset : cells.height
        readonly property real shownOpacity: grid.looks.scroll_bar_color !== undefined
                                             || grid.looks.scroll_bar_image !== undefined ? 1 : 0.5
        opacity: cells.moving || snap.running ? shownOpacity : 0
        Behavior on opacity {
            NumberAnimation { duration: grid.looks.scroll_bar_fade_ms ?? 0 }
        }
        // In the middle of the room beside the last column.
        x: grid.width - grid.sidePadding / 2 - thickness / 2
        y: trackTop + cells.visibleArea.yPosition * trackLength
        width: thickness
        height: cells.visibleArea.heightRatio * trackLength

        BorderImage {
            visible: grid.looks.scroll_bar_image !== undefined
            anchors.fill: parent
            source: visible ? "file://" + grid.themeDirectory + "/" + grid.looks.scroll_bar_image : ""
            border.top: grid.looks.scroll_bar_slice ?? 0
            border.bottom: grid.looks.scroll_bar_slice ?? 0
        }
        Rectangle {
            visible: grid.looks.scroll_bar_image === undefined
            anchors.fill: parent
            radius: width / 2
            // Without a colour of its own: the names' colour.
            color: ThemeValues.color(grid.looks.scroll_bar_color ?? grid.looks.label_color ?? "#ffffff")
        }
    }
}
