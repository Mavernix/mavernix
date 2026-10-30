// One entry of a folder's grid in the dock: its picture, its name (cut in
// the middle if too long, shown whole on the theme's label_expanded while
// the pointer is on it), and the highlight when the keys chose it. The last
// entry, with no file, opens the folder itself.

import QtQuick
import "ThemeValues.js" as ThemeValues

Item {
    id: cell

    // The FolderGrid this is in.
    required property var folderGrid
    required property var icons
    required property int index
    // The entry, or null for the last one, which opens the folder.
    required property var file

    readonly property var looks: folderGrid.looks

    // Above its neighbours while its whole name shows.
    z: wholeName.visible ? 1 : 0

    Fill {
        anchors.fill: parent
        visible: cell.index === cell.folderGrid.selectedIndex && cell.looks.selection !== undefined
        fill: cell.looks.selection ?? ({})
        themeDirectory: cell.folderGrid.themeDirectory
    }

    readonly property bool hasOwnPicture: file === null && looks.open_image !== undefined
    readonly property real pictureSize: hasOwnPicture
        ? (looks.open_image_size ?? folderGrid.iconSize) * folderGrid.cellScale
        : folderGrid.shownIconSize

    Image {
        anchors.horizontalCenter: parent.horizontalCenter
        y: (cell.folderGrid.shownIconSize - cell.pictureSize) / 2
        width: cell.pictureSize
        height: cell.pictureSize
        sourceSize: Qt.size(width, height)
        // Decoded off the shell's main thread: a folder of a hundred programs
        // would otherwise hold the shell still for the best part of a second
        // before it could open. Not Quickshell's stand-in icons
        // ("image://icon/"), whose loader is not known to be thread-safe.
        asynchronous: !source.toString().startsWith("image://icon/")
        source: {
            if (cell.hasOwnPicture) {
                return "file://" + cell.folderGrid.themeDirectory + "/" + cell.looks.open_image
            }
            if (cell.file === null) {
                return cell.icons.path("folder-open")
            }
            return cell.icons.forFolderEntry(cell.file)
        }
        opacity: press.pressed ? 0.6 : 1
    }

    FolderGridText {
        id: name
        folderGrid: cell.folderGrid
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(implicitWidth, (cell.looks.label_width ?? (cell.folderGrid.cellWidth - 8)) * cell.folderGrid.cellScale)
        elide: Text.ElideMiddle
        y: (cell.looks.label_baseline ?? (cell.folderGrid.iconSize + 16)) * cell.folderGrid.cellScale - baselineOffset
        text: cell.file !== null ? cell.file.name : cell.folderGrid.openLabel
        style: cell.looks.label_shadow_color !== undefined ? Text.Raised : Text.Normal
        styleColor: ThemeValues.color(cell.looks.label_shadow_color ?? "#00000000")
    }

    // The whole name, while the pointer is on an entry whose name is cut short.
    readonly property var expandedLooks: looks.label_expanded ?? null
    Item {
        id: wholeName
        visible: press.containsMouse && name.truncated && cell.expandedLooks !== null
        readonly property real padding: cell.expandedLooks?.padding ?? 8
        width: wholeText.implicitWidth + 2 * padding
        height: cell.expandedLooks?.height ?? (wholeText.implicitHeight + 4)
        // Centred on the entry, but moved in rather than cut off at the box's sides.
        x: Math.max(cell.folderGrid.sidePadding / 2 - cell.x,
                    Math.min(cell.folderGrid.width - cell.folderGrid.sidePadding / 2 - width - cell.x,
                             (cell.width - width) / 2))
        y: Math.round(name.y + name.height / 2 - height / 2)

        Fill {
            anchors.fill: parent
            fill: cell.expandedLooks?.fill ?? ({})
            themeDirectory: cell.folderGrid.themeDirectory
        }
        FolderGridText {
            id: wholeText
            folderGrid: cell.folderGrid
            anchors.centerIn: parent
            text: name.text
            color: ThemeValues.color(cell.expandedLooks?.text_color ?? "#000000")
        }
    }

    MouseArea {
        id: press
        anchors.fill: parent
        hoverEnabled: true
        onClicked: cell.folderGrid.open(cell.index)
    }
}
