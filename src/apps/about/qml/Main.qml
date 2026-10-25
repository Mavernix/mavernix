// The "about this computer" window: a picture (or the system's name in
// large type), a line about the system that changes when pressed, rows
// saying what the machine is made of, two buttons and a few closing lines.
//
// Every colour, size and picture is the desktop theme's [about] table
// (docs/themes/programs.md, "About This Computer"); every word, which rows there are and
// what the buttons start is the desktop config's [about] table. The
// window's frame and title bar are the compositor's, like any window's.

import QtQuick
import QtQuick.Window
// The shell's components for theme values: Fill draws any "fill" of a
// theme, PushButton a button from a theme's table, ThemeValues reads its
// colours.
import "../../../shell" as Shell
import "../../../shell/ThemeValues.js" as ThemeValues

Window {
    id: window

    // `settings`, `machine` and `launcher` come from main.cpp.
    readonly property var theme: settings.theme
    readonly property var looks: theme.about ?? ({})
    readonly property var fonts: theme.fonts
    readonly property var wishes: settings.config.about ?? ({})
    readonly property var words: wishes.words ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    // A type the theme names for this window, or its ordinary one.
    function typeFor(name) {
        return fonts[name] ?? fonts.interface
    }

    // --- What is shown ------------------------------------------------------------

    // A line of the config with the machine's facts put in: "{kernel}"
    // becomes the kernel's version. A line that asks for a fact this
    // machine does not tell comes back empty, and is then left out.
    function filled(line) {
        let missing = false
        const text = line.replace(/\{([a-z_]+)\}/g, function (whole, name) {
            const fact = window.factFor(name)
            if (fact === "") {
                missing = true
            }
            return fact
        })
        return missing ? "" : text
    }

    // A fact as the config fixes it ([about.values]), or as the machine
    // tells it.
    function factFor(name) {
        const fixed = (wishes.values ?? ({}))[name]
        return fixed !== undefined ? fixed : machine.fact(name)
    }

    function filledLines(lines) {
        const result = []
        for (const line of lines ?? []) {
            const text = filled(line)
            if (text !== "") {
                result.push(text)
            }
        }
        return result
    }

    // The lines under the picture, of which one is shown at a time; a
    // press goes on to the next.
    readonly property var versionLines: filledLines(wishes.version_lines ?? ["{os_name}", "{kernel}"])
    property int versionIndex: 0

    // Whether the further rows have been asked for with the second button.
    property bool showsMore: false

    // The rows: for each name the config lists, its label and the fact.
    // Rows whose fact this machine does not tell are left out.
    readonly property var rows: {
        let names = wishes.rows ?? ["processor", "memory"]
        if (showsMore) {
            names = names.concat(wishes.more_rows ?? [])
        }
        const result = []
        for (const name of names) {
            const value = factFor(name)
            if (value !== "") {
                result.push({ label: words[name] ?? name, value: value })
            }
        }
        return result
    }

    readonly property var footerLines: filledLines(wishes.footer ?? [])

    // What the second button does: start the config's program, or, when
    // there is none, show the further rows (once).
    readonly property bool moreStartsProgram: (wishes.more_command ?? []).length > 0
    readonly property bool moreHasRows: (wishes.more_rows ?? []).length > 0 && !showsMore
    // A button with nothing to do is left out, and what is under it moves
    // up; or it stays, greyed, if the config wants that.
    readonly property bool keepsIdleButtons: wishes.keep_idle_buttons ?? false

    // --- Places, from the top down ---------------------------------------------------

    readonly property bool hasPicture: looks.picture !== undefined
    readonly property real sideMargin: looks.side_margin ?? 20
    // Without the first button the rows begin where it would have been.
    readonly property real rowsTop: updateButton.visible ? (looks.rows_top ?? 150) : updateButton.y
    readonly property real rowPitch: looks.row_pitch ?? 24
    readonly property real rowHeight: looks.row_height ?? 16
    readonly property real rowsBottom: rowsTop + Math.max(0, rows.length - 1) * rowPitch + rowHeight
    readonly property real footerPitch: looks.footer_pitch ?? 16

    title: wishes.name ?? "About"
    color: ThemeValues.color(looks.background ?? "#f4f4f4")
    // The window cannot be resized: it is as large as what it shows.
    // (The smallest and largest sizes are tied to what is wanted, not to
    // `width` and `height`: those are also set by the compositor, and a
    // size it sets once would then stay for good.)
    readonly property real wantedWidth: looks.width ?? 320
    readonly property real wantedHeight: footer.y + footerLines.length * footerPitch + (looks.bottom_gap ?? 16)
    width: wantedWidth
    height: wantedHeight
    minimumWidth: wantedWidth
    maximumWidth: wantedWidth
    minimumHeight: wantedHeight
    maximumHeight: wantedHeight
    visible: true

    // The picture, where the theme has one.
    Shell.Fill {
        visible: window.hasPicture
        width: (looks.picture_size ?? [128, 128])[0]
        height: (looks.picture_size ?? [128, 128])[1]
        x: Math.round((window.wantedWidth - width) / 2)
        y: looks.picture_top ?? 24
        themeDirectory: window.themeDirectory
        fill: window.hasPicture ? ({ image: looks.picture }) : ({})
    }

    // Without a picture, the config's heading in large type in its place.
    Text {
        visible: !window.hasPicture
        anchors.horizontalCenter: parent.horizontalCenter
        y: looks.heading_top ?? 28
        text: window.filled(wishes.heading ?? "{os_name}")
        color: ThemeValues.color(looks.text_color ?? "#000000")
        font.family: window.typeFor("about_heading").family
        font.pixelSize: window.typeFor("about_heading").size
        font.bold: window.typeFor("about_heading").bold ?? false
    }

    // The line about the system. A press shows the next of its lines.
    Text {
        id: version
        anchors.horizontalCenter: parent.horizontalCenter
        y: looks.version_top ?? 84
        text: window.versionLines.length > 0 ? window.versionLines[window.versionIndex % window.versionLines.length] : ""
        color: ThemeValues.color(looks.version_color ?? looks.text_color ?? "#000000")
        font.family: window.typeFor("about_version").family
        font.pixelSize: window.typeFor("about_version").size

        MouseArea {
            anchors.fill: parent
            onClicked: window.versionIndex += 1
        }
    }

    Shell.PushButton {
        id: updateButton
        looks: window.looks
        fonts: window.fonts
        labelFont: window.typeFor("about_button")
        themeDirectory: window.themeDirectory
        text: words.update ?? "Update…"
        width: Math.max(wantedWidth, looks.button_width ?? 0)
        x: Math.round((window.wantedWidth - width) / 2)
        y: looks.update_button_top ?? 112
        enabled: (wishes.update_command ?? []).length > 0
        visible: enabled || window.keepsIdleButtons
        onClicked: launcher.run(wishes.update_command)
    }

    // The rows: labels in one column with their ends in line, the facts
    // in a second column, and the two columns together in the window's
    // middle. A fact too long for the window is cut short with dots.
    Item {
        id: rowsBlock

        readonly property real gap: looks.row_gap ?? 6
        // The columns are as wide as their widest text.
        property real labelWidth: 0
        property real widestValue: 0
        readonly property real valueWidth:
            Math.min(widestValue, window.width - 2 * window.sideMargin - labelWidth - gap)

        x: Math.round((window.width - (labelWidth + gap + valueWidth)) / 2)
        y: window.rowsTop

        function measure() {
            let label = 0
            let value = 0
            for (let index = 0; index < rowTexts.count; index++) {
                const row = rowTexts.itemAt(index)
                if (row === null) {
                    continue
                }
                label = Math.max(label, Math.ceil(row.labelText.implicitWidth))
                value = Math.max(value, Math.ceil(row.valueText.implicitWidth))
            }
            labelWidth = label
            widestValue = value
        }

        Repeater {
            id: rowTexts
            model: window.rows
            onCountChanged: rowsBlock.measure()

            Item {
                required property var modelData
                required property int index
                readonly property alias labelText: label
                readonly property alias valueText: value

                y: index * window.rowPitch

                Text {
                    id: label
                    width: rowsBlock.labelWidth
                    horizontalAlignment: Text.AlignRight
                    text: modelData.label
                    color: ThemeValues.color(looks.text_color ?? "#000000")
                    font.family: window.typeFor("about_label").family
                    font.pixelSize: window.typeFor("about_label").size
                    font.bold: window.typeFor("about_label").bold ?? true
                    onImplicitWidthChanged: rowsBlock.measure()
                }
                Text {
                    id: value
                    x: rowsBlock.labelWidth + rowsBlock.gap
                    width: rowsBlock.valueWidth
                    elide: Text.ElideRight
                    text: modelData.value
                    color: label.color
                    font.family: window.typeFor("about_value").family
                    font.pixelSize: window.typeFor("about_value").size
                    onImplicitWidthChanged: rowsBlock.measure()
                }
            }
        }
    }

    Shell.PushButton {
        id: moreButton
        looks: window.looks
        fonts: window.fonts
        labelFont: window.typeFor("about_button")
        themeDirectory: window.themeDirectory
        text: words.more ?? "More…"
        width: Math.max(wantedWidth, looks.button_width ?? 0)
        x: Math.round((window.wantedWidth - width) / 2)
        y: window.rowsBottom + (looks.more_button_gap ?? 12)
        enabled: window.moreStartsProgram || window.moreHasRows
        // Once it has shown the further rows it stays, greyed: the window
        // should not lose a button under the pointer that pressed it.
        visible: enabled || window.keepsIdleButtons || window.showsMore
        onClicked: {
            if (window.moreStartsProgram) {
                launcher.run(wishes.more_command)
            } else {
                window.showsMore = true
            }
        }
    }

    // The closing lines.
    Column {
        id: footer
        y: (moreButton.visible ? moreButton.y + moreButton.height : window.rowsBottom) + (looks.footer_gap ?? 12)
        width: window.width

        Repeater {
            model: window.footerLines

            Text {
                required property string modelData
                width: footer.width
                height: window.footerPitch
                horizontalAlignment: Text.AlignHCenter
                text: modelData
                color: ThemeValues.color(looks.footer_color ?? looks.text_color ?? "#000000")
                font.family: window.typeFor("about_footer").family
                font.pixelSize: window.typeFor("about_footer").size
            }
        }
    }
}
