// A folder's contents as a grid of icons in a box pointing at its dock
// tile, with a last entry that opens the folder. A folder with more than
// fits scrolls, by wheel or drag. Looks: the theme's
// [dock.folder.grid] (docs/themes/, "Folders in the dock"); without box
// pictures the box is drawn like a menu's.

import QtQuick
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

    readonly property int entryCount: entries.length + 1
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
        cells.contentY = 0
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
        text: grid.title
        color: ThemeValues.color(grid.looks.title_color ?? grid.looks.label_color ?? "#ffffff")
        anchors.horizontalCenter: parent.horizontalCenter
        // Placed by its baseline.
        y: (grid.looks.title_baseline ?? 18) - baselineOffset
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

        Repeater {
            model: grid.entryCount

            Item {
                id: cell
                required property int index
                readonly property var file: index < grid.entries.length ? grid.entries[index] : null

                x: grid.sidePadding + (index % grid.columns) * grid.shownCellWidth
                y: Math.floor(index / grid.columns) * grid.shownCellHeight
                width: grid.shownCellWidth
                height: grid.shownCellHeight

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
                        // The file's own kind, else its family, else a plain document.
                        const own = themeIcons.find(cell.file.icon)
                        return own !== "" ? own
                                          : themeIcons.path(cell.file.genericIcon, "text-x-generic")
                    }
                    opacity: press.pressed ? 0.6 : 1
                }

                GridText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(implicitWidth,
                                    (grid.looks.label_width ?? (grid.cellWidth - 8)) * grid.cellScale)
                    elide: Text.ElideMiddle
                    y: (grid.looks.label_baseline ?? (grid.iconSize + 16)) * grid.cellScale - baselineOffset
                    text: cell.file !== null ? cell.file.name : grid.openLabel
                    style: grid.looks.label_shadow_color !== undefined ? Text.Raised : Text.Normal
                    styleColor: ThemeValues.color(grid.looks.label_shadow_color ?? "#00000000")
                }

                MouseArea {
                    id: press
                    anchors.fill: parent
                    onClicked: {
                        if (cell.file !== null) {
                            grid.fileChosen(cell.file.path)
                        } else {
                            grid.folderChosen()
                        }
                    }
                }
            }
        }
    }

    // Shows where the view stands while there is more than fits, for a theme
    // that gives it a width.
    Rectangle {
        visible: cells.interactive && thickness > 0
        readonly property real thickness: grid.looks.scroll_bar_width ?? 0
        width: thickness
        radius: thickness / 2
        // Without a colour of its own: the names' colour, at half strength.
        color: ThemeValues.color(grid.looks.scroll_bar_color ?? grid.looks.label_color ?? "#ffffff")
        opacity: grid.looks.scroll_bar_color !== undefined ? 1 : 0.5
        // In the middle of the room beside the last column.
        x: grid.width - grid.sidePadding / 2 - thickness / 2
        y: cells.y + cells.visibleArea.yPosition * cells.height
        height: cells.visibleArea.heightRatio * cells.height
    }
}
