// The dialog that asks before something final is done: shutting down,
// restarting, logging out. One exists for the whole shell; `ask()` shows it.
// Return agrees, Escape cancels; a request with `seconds` agrees by itself
// when they run out. Looks: the theme's [dialog] table and its [window.*] frame.

import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: overlay

    IconSource {
        id: themeIcons
    }

    required property var settings

    readonly property var theme: settings.theme
    readonly property var looks: theme.dialog ?? ({})
    readonly property var frame: theme.window
    readonly property string themeDirectory: settings.theme_directory

    // What is being asked, or null while the dialog is away:
    //   { title, text, button, cancel, icon, seconds, command }
    // An empty `cancel` leaves the cancel button out, for a plain message.
    // `text` may contain %1 for the seconds left. `command` is run on yes.
    // A request may also ask for something to be typed:
    //   field: { placeholder, secret }   a field under the text
    // and may name functions to call instead of a command to run:
    //   agreed(typed)   on yes, with what was typed ("" without a field)
    //   cancelled()     on no
    property var request: null
    property int secondsLeft: 0

    function ask(newRequest) {
        request = newRequest
        secondsLeft = newRequest.seconds ?? 0
    }

    // Runs a config entry's command ({ command, confirm }), asking first if it
    // has a `confirm` table.
    function run(entry) {
        if (entry.confirm === undefined) {
            Quickshell.execDetached(entry.command)
            return
        }
        const question = Object.assign({}, entry.confirm)
        question.command = entry.command
        ask(question)
    }

    function agree() {
        // Return reaches both the field and the dialog; the second finds nothing to do.
        if (request === null) {
            return
        }
        const answered = request
        const typed = field.text
        field.text = ""
        request = null
        if (answered.agreed !== undefined) {
            answered.agreed(typed)
        } else {
            Quickshell.execDetached(answered.command)
        }
    }

    function cancel() {
        const answered = request
        field.text = ""
        request = null
        if (answered !== null && answered.cancelled !== undefined) {
            answered.cancelled()
        }
    }

    // Takes the dialog away without calling anyone, for a requester that has
    // had its answer from elsewhere.
    function dismiss() {
        field.text = ""
        request = null
    }

    visible: request !== null

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    color: "transparent"

    Timer {
        running: overlay.request !== null && overlay.secondsLeft > 0
        interval: 1000
        repeat: true
        onTriggered: {
            overlay.secondsLeft -= 1
            if (overlay.secondsLeft === 0) {
                overlay.agree()
            }
        }
    }

    // --- Sizes ---------------------------------------------------------------

    readonly property real titleBarHeight: (looks.title_bar ?? false) ? frame.titlebar.height : 0
    readonly property var iconInset: looks.icon_inset ?? [12, 12]
    readonly property var buttonInset: looks.button_inset ?? [12, 12]

    readonly property real panelHeight: theme.panel?.height ?? 0

    Item {
        id: box

        width: overlay.looks.width ?? 420
        height: overlay.titleBarHeight + content.height
        x: Math.floor((overlay.width - width) / 2)
        // The theme's top_share of the free height lies above the dialog.
        y: Math.round(overlay.panelHeight
                      + (overlay.height - overlay.panelHeight - height) * (overlay.looks.top_share ?? 0.5))

        focus: true
        Keys.onReturnPressed: overlay.agree()
        Keys.onEnterPressed: overlay.agree()
        Keys.onEscapePressed: overlay.cancel()

        RectangularShadow {
            visible: overlay.frame.shadow !== undefined
            anchors.fill: parent
            radius: overlay.frame.frame.corner_radius ?? 0
            color: ThemeValues.color(overlay.frame.shadow?.color)
            blur: overlay.frame.shadow?.blur_radius ?? 0
            offset: Qt.vector2d(overlay.frame.shadow?.offset[0] ?? 0, overlay.frame.shadow?.offset[1] ?? 0)
        }

        // The hairline's three colours are for the top, the sides and the bottom.
        readonly property var rim: overlay.frame.frame.rim ?? []
        Rectangle {
            visible: box.rim.length === 3
            x: -1; y: -1; width: box.width + 2; height: 1
            color: ThemeValues.color(box.rim[0])
        }
        Rectangle {
            visible: box.rim.length === 3
            x: -1; y: 0; width: 1; height: box.height
            color: ThemeValues.color(box.rim[1])
        }
        Rectangle {
            visible: box.rim.length === 3
            x: box.width; y: 0; width: 1; height: box.height
            color: ThemeValues.color(box.rim[1])
        }
        Rectangle {
            visible: box.rim.length === 3
            x: -1; y: box.height; width: box.width + 2; height: 1
            color: ThemeValues.color(box.rim[2])
        }

        Item {
            id: inside
            anchors.fill: parent
            layer.enabled: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: cornerShape
            }

            Rectangle {
                anchors.fill: parent
                color: ThemeValues.color(overlay.looks.background ?? "#f0f0f0")
            }

            // The empty title bar, with the frame's light line on top and its divider below.
            Item {
                visible: overlay.titleBarHeight > 0
                width: parent.width
                height: overlay.titleBarHeight

                Fill {
                    anchors.fill: parent
                    fill: overlay.frame.titlebar.background
                    themeDirectory: overlay.themeDirectory
                }
                Column {
                    Repeater {
                        model: overlay.frame.titlebar.highlight ?? []
                        Rectangle {
                            required property string modelData
                            width: inside.width; height: 1
                            color: ThemeValues.color(modelData)
                        }
                    }
                }
                Column {
                    anchors.bottom: parent.bottom
                    Repeater {
                        model: overlay.frame.titlebar.separator ?? []
                        Rectangle {
                            required property string modelData
                            width: inside.width; height: 1
                            color: ThemeValues.color(modelData)
                        }
                    }
                }
            }

            Item {
                id: content
                y: overlay.titleBarHeight
                width: parent.width
                height: buttons.y + buttons.height + overlay.buttonInset[1]

                Image {
                    id: picture
                    x: overlay.iconInset[0]
                    y: overlay.iconInset[1]
                    width: overlay.looks.icon_size ?? 64
                    height: width
                    sourceSize: Qt.size(width, height)
                    source: {
                        const name = overlay.request?.icon ?? ""
                        if (name === "") {
                            return ""
                        }
                        const own = (overlay.looks.icons ?? ({}))[name]
                        return own !== undefined ? "file://" + overlay.themeDirectory + "/" + own
                                                 : themeIcons.find(name)
                    }
                }

                // A text's ink starts a little inside its box.
                readonly property real textInkInset: 2

                Text {
                    id: title
                    x: (overlay.looks.text_left ?? 90) + content.textInkInset
                    y: overlay.looks.title_top ?? 16
                    width: (overlay.looks.text_width ?? 300) - 2 * content.textInkInset
                    wrapMode: Text.WordWrap
                    text: overlay.request?.title ?? ""
                    color: ThemeValues.color(overlay.looks.text_color ?? "#000000")
                    font.family: overlay.theme.fonts.dialog_title.family
                    font.pixelSize: overlay.theme.fonts.dialog_title.size
                    font.bold: overlay.theme.fonts.dialog_title.bold ?? false
                    lineHeightMode: Text.FixedHeight
                    lineHeight: overlay.looks.title_line_height ?? 17
                }

                Text {
                    id: explanation
                    x: title.x
                    y: title.y + title.height + (overlay.looks.text_gap ?? 8)
                    width: title.width
                    wrapMode: Text.WordWrap
                    text: (overlay.request?.text ?? "").replace("%1", overlay.secondsLeft)
                    color: title.color
                    font.family: overlay.theme.fonts.dialog_text.family
                    font.pixelSize: overlay.theme.fonts.dialog_text.size
                    lineHeightMode: Text.FixedHeight
                    lineHeight: overlay.looks.text_line_height ?? 14
                }

                // A field, if the request asks for one; it looks like the lock screen's field.
                Item {
                    id: fieldBox
                    readonly property var fieldLooks: (overlay.theme.lock ?? ({})).field ?? ({})
                    readonly property bool wanted: overlay.request?.field !== undefined
                    visible: wanted
                    x: title.x
                    y: explanation.y + explanation.height + (overlay.looks.text_gap ?? 8)
                    width: title.width
                    height: wanted ? (fieldLooks.height ?? 22) : 0

                    Repeater {
                        model: fieldBox.fieldLooks.focus_rings ?? []
                        Rectangle {
                            required property int index
                            required property string modelData
                            anchors.fill: parent
                            anchors.margins: -(index + 1)
                            color: "transparent"
                            border.width: 1
                            border.color: ThemeValues.color(modelData)
                            radius: (fieldBox.fieldLooks.fill?.radius ?? 0) + index + 1
                        }
                    }
                    Fill {
                        anchors.fill: parent
                        fill: fieldBox.fieldLooks.fill ?? ({ color: "#ffffff", border: { width: 1, color: "#909090" } })
                        themeDirectory: overlay.themeDirectory
                    }
                    Text {
                        visible: field.text === ""
                        x: field.x
                        anchors.verticalCenter: parent.verticalCenter
                        text: overlay.request?.field?.placeholder ?? ""
                        color: ThemeValues.color(fieldBox.fieldLooks.placeholder_color ?? "#808080")
                        font: field.font
                    }
                    TextInput {
                        id: field
                        x: fieldBox.fieldLooks.text_inset ?? 4
                        width: parent.width - 2 * x
                        anchors.verticalCenter: parent.verticalCenter
                        clip: true
                        echoMode: (overlay.request?.field?.secret ?? false) ? TextInput.Password : TextInput.Normal
                        passwordMaskDelay: 0
                        color: ThemeValues.color(overlay.looks.text_color ?? "#000000")
                        font.family: overlay.theme.fonts.interface.family
                        font.pixelSize: overlay.theme.fonts.interface.size
                        onAccepted: overlay.agree()
                        Keys.onEscapePressed: overlay.cancel()
                    }
                }
                Connections {
                    target: overlay
                    function onRequestChanged() {
                        if (fieldBox.wanted) {
                            field.forceActiveFocus()
                        }
                    }
                }

                Row {
                    id: buttons
                    y: Math.max(fieldBox.y + fieldBox.height,
                                picture.y + picture.height - (overlay.looks.button_gap_above ?? 8))
                       + (overlay.looks.button_gap_above ?? 8)
                    x: content.width - overlay.buttonInset[0] - width
                    // Buttons whose pictures bring their own room touch; plain ones get a gap.
                    spacing: overlay.looks.button_spacing ?? 0

                    readonly property real buttonWidth: Math.max(cancelButton.wantedWidth, agreeButton.wantedWidth)

                    PushButton {
                        id: cancelButton
                        looks: overlay.looks
                        fonts: overlay.theme.fonts
                        themeDirectory: overlay.themeDirectory
                        width: buttons.buttonWidth
                        text: overlay.request?.cancel ?? ""
                        // A message with only "OK" has no cancel.
                        visible: text !== ""
                        onClicked: overlay.cancel()
                    }
                    PushButton {
                        id: agreeButton
                        looks: overlay.looks
                        fonts: overlay.theme.fonts
                        themeDirectory: overlay.themeDirectory
                        width: buttons.buttonWidth
                        isDefault: true
                        text: overlay.request?.button ?? ""
                        onClicked: overlay.agree()
                    }
                }
            }
        }

        Rectangle {
            id: cornerShape
            anchors.fill: parent
            visible: false
            layer.enabled: true
            radius: overlay.frame.frame.corner_radius ?? 0
        }
    }
}
