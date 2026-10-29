// One app in the Installed and Updates lists: its icon, name, version and
// what's new, and its button. On the Installed list it also has Remove.
// What went wrong the last time, if anything, shows under the name.

import QtQuick

Item {
    id: row

    required property var looks
    required property var app
    property bool removable: false

    signal chosen()

    readonly property string failure: store.failures[app.name] ?? ""

    height: looks.value("row_height")

    Image {
        id: icon
        anchors.verticalCenter: parent.verticalCenter
        width: row.looks.value("row_icon_size")
        height: width
        sourceSize: Qt.size(width * 2, height * 2)
        source: "file://" + row.app.icon
        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: row.chosen()
        }
    }

    Column {
        anchors.left: icon.right
        anchors.leftMargin: 16
        anchors.right: buttons.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        StoreText {
            width: parent.width
            looks: row.looks
            kind: "name"
            text: row.app.title
        }

        StoreText {
            width: parent.width
            looks: row.looks
            kind: "small"
            color: row.looks.color("secondary_text_color")
            text: row.looks.word("version") + " " + (row.app.installed_version || row.app.version)
        }

        StoreText {
            width: parent.width
            looks: row.looks
            kind: "small"
            visible: text !== ""
            color: row.looks.color(row.failure !== "" ? "failure_text_color" : "secondary_text_color")
            // An update says what it brings; a failure says what went wrong.
            text: row.failure !== "" ? row.failure : (row.app.installed_version !== row.app.version ? row.app.whats_new : "")
            maximumLineCount: 2
            wrapMode: Text.Wrap
        }
    }

    Row {
        id: buttons
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        StoreText {
            anchors.verticalCenter: parent.verticalCenter
            visible: row.removable && store.jobs[row.app.name] === undefined
            looks: row.looks
            kind: "small"
            color: row.looks.color("secondary_text_color")
            text: row.looks.word("remove")
            font.underline: removeArea.containsMouse

            MouseArea {
                id: removeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: store.remove(row.app.name)
            }
        }

        StoreButton {
            anchors.verticalCenter: parent.verticalCenter
            looks: row.looks
            app: row.app
            small: true
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: row.looks.color("separator_color")
    }
}
