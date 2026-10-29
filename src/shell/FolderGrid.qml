// A folder's contents as a grid of icons in a box pointing at its dock
// tile, with a last entry that opens the folder. A folder with more than
// fits on the screen scrolls, by wheel or drag. Looks: the theme's
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
    // The most room the box may take; rows beyond it scroll.
    property real mostWidth: Infinity
    property real mostHeight: Infinity

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

    readonly property int entryCount: entries.length + 1
    // As many columns as make the grid about twice as wide as it is high,
    // but no more than fit beside each other.
    readonly property real columnsThatFit: Math.max(1, Math.floor((mostWidth - 2 * sidePadding) / cellWidth))
    readonly property int columns:
        Math.max(1, Math.min(entryCount, columnsThatFit, Math.ceil(Math.sqrt(2 * entryCount))))
    readonly property int rows: Math.ceil(entryCount / columns)
    readonly property real rowsThatFit:
        Math.max(1, Math.floor((mostHeight - firstRowTop - bottomPadding) / cellHeight))
    readonly property int shownRows: Math.min(rows, rowsThatFit)

    width: 2 * sidePadding + columns * cellWidth
    height: firstRowTop + shownRows * cellHeight + bottomPadding

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
        height: grid.shownRows * grid.cellHeight
        contentHeight: grid.rows * grid.cellHeight
        interactive: grid.rows > grid.shownRows
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Repeater {
            model: grid.entryCount

            Item {
                id: cell
                required property int index
                readonly property var file: index < grid.entries.length ? grid.entries[index] : null

                x: grid.sidePadding + (index % grid.columns) * grid.cellWidth
                y: Math.floor(index / grid.columns) * grid.cellHeight
                width: grid.cellWidth
                height: grid.cellHeight

                readonly property bool hasOwnPicture: file === null && grid.looks.open_image !== undefined
                readonly property real pictureSize:
                    hasOwnPicture ? (grid.looks.open_image_size ?? grid.iconSize) : grid.iconSize
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: (grid.iconSize - cell.pictureSize) / 2
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
                    width: Math.min(implicitWidth, grid.looks.label_width ?? (grid.cellWidth - 8))
                    elide: Text.ElideMiddle
                    y: (grid.looks.label_baseline ?? (grid.iconSize + 16)) - baselineOffset
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

    // Shows where the view stands while there is more than fits.
    Rectangle {
        visible: cells.interactive
        readonly property real thickness: grid.looks.scroll_bar_width ?? 4
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
