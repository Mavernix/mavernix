// One app in the Featured grid: its icon, name and category, and the small
// button that gets it. Pressing the icon or the name opens the app's page.

import QtQuick

Item {
    id: tile

    required property var looks
    required property var app

    signal chosen()

    width: looks.value("tile_width")
    height: looks.value("tile_height")

    Image {
        id: icon
        width: tile.looks.value("tile_icon_size")
        height: width
        sourceSize: Qt.size(width * 2, height * 2)
        source: "file://" + tile.app.icon
        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: tile.chosen()
        }
    }

    Column {
        anchors.left: icon.right
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 8
        spacing: 2

        StoreText {
            width: parent.width
            looks: tile.looks
            kind: "name"
            text: tile.app.title

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: tile.chosen()
            }
        }

        StoreText {
            width: parent.width
            looks: tile.looks
            kind: "small"
            color: tile.looks.color("secondary_text_color")
            text: tile.app.category
        }

        // The gap above the button.
        Item { width: 1; height: 10 }

        StoreButton {
            looks: tile.looks
            app: tile.app
            small: true
        }
    }
}
