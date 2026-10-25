// The name bubble shown above a dock tile while the pointer is on it. It
// appears at once and fades out. Looks: [dock.label] and [fonts].dock_label.

import QtQuick
import "ThemeValues.js" as ThemeValues

Item {
    id: label

    // The theme's [dock.label] table and [fonts].dock_label.
    required property var looks
    required property var font

    property string text: ""
    property bool wanted: false

    width: bubble.width
    height: bubble.height + looks.pointer_height

    opacity: wanted && text !== "" ? 1 : 0
    Behavior on opacity {
        enabled: !label.wanted
        NumberAnimation {
            duration: label.looks.fade_out_ms
        }
    }
    visible: opacity > 0

    Rectangle {
        id: bubble
        width: name.implicitWidth + 2 * label.looks.padding
        height: label.looks.height
        radius: height / 2
        color: ThemeValues.color(label.looks.background)

        Text {
            id: name
            anchors.centerIn: parent
            text: label.text
            color: ThemeValues.color(label.looks.text_color)
            font.family: label.font.family
            font.pixelSize: label.font.size
            font.bold: label.font.bold ?? false
        }
    }

    // The bubble's pointer, tip downwards.
    Canvas {
        anchors.top: bubble.bottom
        anchors.horizontalCenter: bubble.horizontalCenter
        width: label.looks.pointer_width
        height: label.looks.pointer_height

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.beginPath()
            context.moveTo(0, 0)
            context.lineTo(width, 0)
            context.lineTo(width / 2, height)
            context.closePath()
            context.fillStyle = ThemeValues.color(label.looks.background)
            context.fill()
        }
    }
}
