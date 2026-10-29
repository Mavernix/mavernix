// Shows where a folder's grid in the dock stands while it moves, for a theme
// that gives the bar a width ([dock.folder.grid] scroll_bar_width): the
// theme's picture stretched along its middle (scroll_bar_image,
// scroll_bar_slice), else a bar in a colour. It runs along the cells, or
// along the box less scroll_bar_inset at each end.

import QtQuick
import "ThemeValues.js" as ThemeValues

Item {
    id: bar

    // The FolderGrid this is in, the Flickable it follows (view), and whether the
    // view is still coming to rest after a move.
    required property var folderGrid
    required property Flickable view
    property bool settling: false

    readonly property var looks: folderGrid.looks
    readonly property real thickness: looks.scroll_bar_width ?? 0
    readonly property bool alongBox: looks.scroll_bar_inset !== undefined
    readonly property real trackTop: alongBox ? looks.scroll_bar_inset : view.y
    readonly property real trackLength: alongBox ? folderGrid.height - 2 * looks.scroll_bar_inset : view.height
    readonly property real shownOpacity:
        looks.scroll_bar_color !== undefined || looks.scroll_bar_image !== undefined ? 1 : 0.5

    visible: view.interactive && thickness > 0
    opacity: view.moving || settling ? shownOpacity : 0
    Behavior on opacity {
        NumberAnimation { duration: bar.looks.scroll_bar_fade_ms ?? 0 }
    }

    // In the middle of the room beside the last column.
    x: folderGrid.width - folderGrid.sidePadding / 2 - thickness / 2
    y: trackTop + view.visibleArea.yPosition * trackLength
    width: thickness
    height: view.visibleArea.heightRatio * trackLength

    BorderImage {
        visible: bar.looks.scroll_bar_image !== undefined
        anchors.fill: parent
        source: visible ? "file://" + bar.folderGrid.themeDirectory + "/" + bar.looks.scroll_bar_image : ""
        border.top: bar.looks.scroll_bar_slice ?? 0
        border.bottom: bar.looks.scroll_bar_slice ?? 0
    }
    Rectangle {
        visible: bar.looks.scroll_bar_image === undefined
        anchors.fill: parent
        radius: width / 2
        // Without a colour of its own: the names' colour.
        color: ThemeValues.color(bar.looks.scroll_bar_color ?? bar.looks.label_color ?? "#ffffff")
    }
}
