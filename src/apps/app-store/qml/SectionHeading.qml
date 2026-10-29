// A heading over a part of a page, with a line under it, as in OS X's store.
// Whatever is placed inside sits at its right end (UPDATE ALL, for one).

import QtQuick

Item {
    id: heading

    required property var looks
    property alias text: label.text
    default property alias rightEnd: rightEnd.data

    height: looks.value("heading_height")

    StoreText {
        id: label
        looks: heading.looks
        kind: "heading"
        anchors.left: parent.left
        anchors.right: rightEnd.left
        anchors.verticalCenter: parent.verticalCenter
    }

    Row {
        id: rightEnd
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: heading.looks.color("separator_color")
    }
}
