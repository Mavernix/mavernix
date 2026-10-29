// UPDATE ALL, at the top of the Updates page: puts every listed app in the queue.

import QtQuick
import "../../../shell" as Shell

Item {
    id: button

    required property var looks
    required property var apps

    implicitWidth: label.implicitWidth + 2 * looks.value("button_padding")
    implicitHeight: looks.value("small_button_height")

    Shell.Fill {
        anchors.fill: parent
        themeDirectory: button.looks.themeDirectory
        fill: button.looks.value(area.pressed && area.containsMouse ? "button_pressed" : "button")
    }

    StoreText {
        id: label
        anchors.centerIn: parent
        looks: button.looks
        kind: "button"
        color: button.looks.color("button_text_color")
        text: button.looks.word("update_all")
    }

    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: {
            for (const app of button.apps) {
                store.install(app.name)
            }
        }
    }
}
