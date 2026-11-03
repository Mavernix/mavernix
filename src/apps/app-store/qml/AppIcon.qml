// An app's or extension's icon. One without an icon of its own, or whose
// picture can't be read, shows the icon theme's plain program (main.cpp's
// "theme-icon" provider).

import QtQuick

Image {
    id: icon

    required property var app

    readonly property string plainIcon: "image://theme-icon/application-x-executable"
    property bool unreadable: false

    sourceSize: Qt.size(width * 2, height * 2)
    source: (app.icon ?? "") !== "" && !unreadable ? "file://" + app.icon : plainIcon
    fillMode: Image.PreserveAspectFit

    onAppChanged: unreadable = false
    onStatusChanged: {
        if (status === Image.Error && source.toString() !== plainIcon) {
            unreadable = true
        }
    }
}
