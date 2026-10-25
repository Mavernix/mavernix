// A push button for the shell's own dialogs. Its looks come from a theme
// table shaped like [dialog]: fills for plain, pressed, disabled and
// default states, plus a few sizes.

import QtQuick
import "ThemeValues.js" as ThemeValues

Item {
    id: button

    required property var looks
    required property var fonts
    required property string themeDirectory
    property var labelFont: fonts.interface

    property string text: ""
    // The button that the Return key presses; themes usually colour it.
    property bool isDefault: false

    signal clicked()

    // A dialog makes all its buttons as wide as the widest wantedWidth.
    readonly property real wantedWidth:
        Math.max(looks.button_min_width ?? 0, Math.ceil(label.implicitWidth) + 2 * (looks.button_padding ?? 12))
    height: looks.button_height ?? 24

    readonly property bool pressed: area.pressed && area.containsMouse

    Fill {
        anchors.fill: parent
        themeDirectory: button.themeDirectory
        fill: {
            let wanted = (button.isDefault ? "button_default" : "button") + (button.pressed ? "_pressed" : "")
            if (!button.enabled) {
                wanted = "button_disabled"
            }
            // A theme may leave out every look but the plain one.
            return button.looks[wanted] ?? button.looks.button ?? ({})
        }
    }

    Text {
        id: label
        text: button.text
        color: {
            if (!button.enabled) {
                return ThemeValues.color(button.looks.button_disabled_text_color ?? button.looks.text_color)
            }
            return ThemeValues.color(button.isDefault ? (button.looks.button_default_text_color ?? button.looks.text_color)
                                                      : button.looks.text_color)
        }
        font.family: button.labelFont.family
        font.pixelSize: button.labelFont.size
        font.bold: button.labelFont.bold ?? false
        anchors.horizontalCenter: parent.horizontalCenter
        // The theme may fix the baseline row; otherwise the label is centred.
        y: button.looks.button_text_baseline_row !== undefined
            ? button.looks.button_text_baseline_row - baselineOffset
            : (parent.height - height) / 2
    }

    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: button.clicked()
    }
}
