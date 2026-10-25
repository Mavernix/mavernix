// The second page: where the installer is chosen, by dropping it on the
// well or with Choose….

import QtQuick
import Qt.labs.platform as Platform
import "../../../shell" as Shell
import "../../../shell/ThemeValues.js" as ThemeValues

Item {
    readonly property string heading: window.words.select_heading ?? "Select the Installer"
    readonly property var chosen: window.installer !== "" ? job.describe(window.installer) : ({})
    // Whether the chosen file is still being read to see which look it makes.
    readonly property bool reading: job.identifying && job.identifiedPath === window.installer

    // The first local path among dropped addresses, or "".
    function droppedPath(drop) {
        const lines = text => text.split(/\r?\n/).filter(line => line !== "" && !line.startsWith("#"))
        let addresses = lines(drop.getDataAsString("text/uri-list"))
        // Without a desktop service the address list can come back empty,
        // with the paths in the plain text beside it.
        if (addresses.length === 0) {
            addresses = lines(drop.getDataAsString("text/plain"))
        }
        for (const address of addresses) {
            if (address.startsWith("file://")) {
                return decodeURIComponent(address.substring("file://".length))
            }
            if (address.startsWith("/")) {
                return address
            }
        }
        return ""
    }

    Shell.Fill {
        id: well
        anchors { top: parent.top; left: parent.left; right: parent.right }
        // Taller than the theme says when the words about the file need it.
        height: Math.max(window.looks.drop_well_height, wellContent.implicitHeight + 2 * window.looks.pane_padding)
        fill: dropArea.containsDrag || window.installer !== "" ? window.looks.drop_well_ready : window.looks.drop_well
        themeDirectory: window.themeDirectory

        DropArea {
            id: dropArea
            anchors.fill: parent
            onDropped: drop => {
                const path = droppedPath(drop)
                if (path !== "") {
                    window.installer = path
                    drop.accept(Qt.CopyAction)
                }
            }
        }

        Column {
            id: wellContent
            anchors.centerIn: parent
            width: parent.width - 2 * window.looks.pane_padding
            spacing: window.looks.paragraph_gap

            Image {
                visible: window.looks.drop_well_picture !== undefined
                anchors.horizontalCenter: parent.horizontalCenter
                width: window.looks.drop_well_icon_size
                height: width
                sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                source: visible ? "file://" + window.themeDirectory + "/" + window.looks.drop_well_picture : ""
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideMiddle
                text: chosen.name ?? (window.words.drop_here ?? "Drop the installer here")
                font.family: window.fonts.theme_installer_text.family
                font.pixelSize: window.fonts.theme_installer_text.size
                font.bold: true
                color: ThemeValues.color(window.looks.text_color)
            }
            BodyText {
                horizontalAlignment: Text.AlignHCenter
                secondary: true
                text: chosen.name !== undefined ? (chosen.size ?? "")
                    : job.looks.length === 1 ? job.looks[0].drop_hint
                    : (window.words.drop_hint ?? "An installer for one of the looks on the first page")
            }
            // While the chosen file is read: that it is, and that it can take a while.
            BodyText {
                visible: reading
                horizontalAlignment: Text.AlignHCenter
                font.bold: true
                text: window.words.reading ?? "Reading the installer…"
            }
            BusyBar {
                visible: reading
            }
            BodyText {
                visible: reading
                horizontalAlignment: Text.AlignHCenter
                secondary: true
                text: window.words.reading_note ??
                    "A large image on a slow disk can take a few minutes. You can choose another file meanwhile."
            }
            // What the chosen file makes, or why it makes nothing.
            BodyText {
                visible: window.installer !== "" && !reading
                horizontalAlignment: Text.AlignHCenter
                font.bold: window.chosenLook !== null
                color: ThemeValues.color(window.chosenLook !== null ? window.looks.stage_done_color
                                                                   : window.looks.stage_failed_color)
                text: window.chosenLook === null ? (window.identified.problem ?? "")
                    : window.chosenLook.installed
                    ? (window.words.makes_installed ?? "Makes %1, which is installed already; installing it again replaces it.").arg(window.chosenLook.name)
                    : (window.words.makes ?? "Makes %1.").arg(window.chosenLook.name)
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: window.theme.dialog.button_spacing ?? 8

                Shell.PushButton {
                    looks: window.theme.dialog
                    fonts: window.fonts
                    themeDirectory: window.themeDirectory
                    width: wantedWidth
                    text: chosen.name !== undefined ? (window.words.change ?? "Change…") : (window.words.choose ?? "Choose…")
                    onClicked: chooser.open()
                }
                // Stops reading the file; the well is empty again.
                Shell.PushButton {
                    visible: reading
                    looks: window.theme.dialog
                    fonts: window.fonts
                    themeDirectory: window.themeDirectory
                    width: wantedWidth
                    text: window.words.stop ?? "Stop"
                    onClicked: window.installer = ""
                }
            }
        }
    }

    BodyText {
        anchors { top: well.bottom; topMargin: window.looks.paragraph_gap }
        secondary: true
        text: window.words.select_note ??
            "The import takes a few minutes and needs about 5 GB of free space while it runs."
    }

    Platform.FileDialog {
        id: chooser
        title: window.words.choose_title ?? "Choose the Installer"
        nameFilters: [(window.words.installer_files ?? "Installer images") + " (*.dmg *.iso *.cdr)"]
        onAccepted: window.installer = decodeURIComponent(file.toString().replace(/^file:\/\//, ""))
    }
}
