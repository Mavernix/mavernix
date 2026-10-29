// The search field at the toolbar's right: the theme's rounded field with
// its magnifier (search_icon) at the left. Escape empties it.

import QtQuick
import "../../../shell" as Shell

Item {
    id: field

    required property var looks
    property alias text: input.text

    width: looks.value("search_field_size")[0]
    height: looks.value("search_field_size")[1]

    Shell.Fill {
        anchors.fill: parent
        themeDirectory: field.looks.themeDirectory
        fill: field.looks.value("search_field")
    }

    Image {
        id: icon
        x: Math.round((field.height - width) / 2) + 2
        anchors.verticalCenter: parent.verticalCenter
        source: field.looks.picture("search_icon")
        visible: source != ""
    }

    TextInput {
        id: input
        anchors.left: icon.visible ? icon.right : parent.left
        anchors.leftMargin: icon.visible ? 4 : field.height / 2
        anchors.right: parent.right
        anchors.rightMargin: field.height / 2
        anchors.verticalCenter: parent.verticalCenter
        readonly property var themeFont: field.looks.font("text")
        font.family: themeFont.family
        font.pixelSize: themeFont.size
        color: field.looks.color("search_text_color")
        selectByMouse: true
        clip: true
        Keys.onEscapePressed: text = ""

        StoreText {
            anchors.fill: parent
            visible: input.text === "" && !input.activeFocus
            looks: field.looks
            color: field.looks.color("secondary_text_color")
            text: field.looks.word("search")
            verticalAlignment: Text.AlignVCenter
        }
    }
}
