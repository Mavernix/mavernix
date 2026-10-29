// Back to the folder before, in a folder opened inside a folder's grid in
// the dock: the theme's [dock.folder.grid] back_button picture, else a plain
// arrow in the names' colour.

import QtQuick

Item {
    id: button

    // The FolderGrid this is in.
    required property var folderGrid
    signal clicked()

    readonly property var looks: folderGrid.looks.back_button ?? ({})

    // The plain arrow is small, so its button is at least this big to hit.
    readonly property real leastPlainSize: 24

    x: looks.left ?? folderGrid.sidePadding
    y: looks.top ?? 4
    width: picture.visible ? picture.implicitWidth : Math.max(leastPlainSize, arrow.implicitWidth + 8)
    height: picture.visible ? picture.implicitHeight : Math.max(leastPlainSize, arrow.implicitHeight)

    Image {
        id: picture
        visible: button.looks.image !== undefined
        source: !visible ? "" : "file://" + button.folderGrid.themeDirectory + "/"
            + (press.pressed ? (button.looks.pressed_image ?? button.looks.image) : button.looks.image)
    }
    FolderGridText {
        id: arrow
        folderGrid: button.folderGrid
        visible: !picture.visible
        anchors.centerIn: parent
        text: "‹"
        // Bigger than the names, so it reads as a button.
        font.pixelSize: Math.round(button.folderGrid.font.size * 2)
        opacity: press.pressed ? 0.6 : 1
    }
    MouseArea {
        id: press
        anchors.fill: parent
        onClicked: button.clicked()
    }
}
