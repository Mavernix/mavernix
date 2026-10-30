// The Theme Installer's small window at login (--whats-new): what a new
// import would bring the looks the user imported, one sentence each, with
// Later and Import Again. It shows nothing, and ends, when there is nothing
// new. Which looks have something new and what is the back end's
// (backend/updates.py).
//
// Its width is the theme's [theme_installer] news_width; its height follows
// the text. Words can be replaced in the config's [theme_installer.words].
// `settings` and `job` come from main.cpp.

import QtQml
import QtQuick
import QtQuick.Window
import "../../../shell" as Shell
import "../../../shell/ThemeValues.js" as ThemeValues

Window {
    id: window

    readonly property var theme: settings.theme
    readonly property var looks: theme.theme_installer
    readonly property var fonts: theme.fonts
    readonly property string themeDirectory: settings.theme_directory
    readonly property var words: (settings.config.theme_installer ?? ({})).words ?? ({})

    // The imported looks to offer a new import of (the back end decides).
    readonly property var newsLooks: job.looks.filter(look => look.offer_import)
    readonly property string lookNames: newsLooks.map(look => look.name).join(", ")

    // Shown once the check has said there is something new; without
    // anything new there is nothing to show.
    visible: !job.checking && newsLooks.length > 0
    Connections {
        target: job
        function onChecked() {
            if (window.newsLooks.length === 0) {
                Qt.quit()
            }
        }
    }

    title: words.window_title ?? "Theme Installer"
    // A theme from before this window: as wide as the installer's own.
    width: looks.news_width ?? looks.width
    minimumWidth: width
    maximumWidth: width
    // Only a least height: a most height tied to the height would hold the
    // window at its first, empty size when the list arrives.
    readonly property real wantedHeight: content.implicitHeight + 2 * looks.content_margin
    height: wantedHeight
    minimumHeight: wantedHeight
    color: "transparent"

    // Closing the window without an answer counts as Later, so that it does
    // not come back at every login.
    property bool answered: false
    onClosing: {
        if (!answered) {
            putOff()
        }
    }

    function putOff() {
        job.putOff(newsLooks.map(look => look.id))
    }

    function later() {
        answered = true
        putOff()
        close()
    }

    function importAgain() {
        answered = true
        job.openInstaller(job.lastInstaller)
        close()
    }

    Shell.Fill {
        anchors.fill: parent
        fill: looks.background
        themeDirectory: window.themeDirectory
    }

    // The installer's picture at the left, the text and buttons beside it.
    Image {
        id: picture
        x: looks.content_margin
        y: looks.content_margin
        width: looks.news_icon_size ?? looks.drop_well_icon_size
        height: width
        sourceSize: Qt.size(width, height)
        visible: looks.drop_well_picture !== undefined
        source: visible ? "file://" + window.themeDirectory + "/" + looks.drop_well_picture : ""
    }

    Column {
        id: content
        x: picture.visible ? picture.x + picture.width + looks.content_margin : looks.content_margin
        y: looks.content_margin
        width: window.width - x - looks.content_margin
        spacing: looks.paragraph_gap

        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: (window.words.news_heading ?? "%1 has new features").arg(window.lookNames)
            font.family: fonts.theme_installer_heading.family
            font.pixelSize: fonts.theme_installer_heading.size
            font.bold: fonts.theme_installer_heading.bold ?? false
            color: ThemeValues.color(looks.text_color)
        }

        BodyText {
            text: job.lastInstaller !== ""
                ? (window.words.news_text_known ??
                   "Import %1 again to get them. The installer you used last time is still there.")
                      .arg(window.lookNames)
                : (window.words.news_text ??
                   "Import %1 again to get them. You'll need the installer you imported it from.")
                      .arg(window.lookNames)
        }

        // One sentence per feature, each with a bullet; a sentence that
        // wraps keeps to the right of its bullet.
        Column {
            width: parent.width
            spacing: looks.news_item_gap ?? 0
            Repeater {
                model: window.newsLooks.reduce((all, look) => all.concat(look.import_brings), [])
                delegate: Row {
                    id: item
                    required property string modelData
                    width: parent.width

                    BodyText {
                        id: bullet
                        width: implicitWidth
                        wrapMode: Text.NoWrap
                        // About two spaces' room; spaces at a text's end
                        // add nothing to its width.
                        rightPadding: Math.round(0.6 * font.pixelSize)
                        text: "•"
                    }
                    BodyText {
                        width: item.width - bullet.width
                        text: item.modelData
                    }
                }
            }
        }

        // Later, then Import Again at the right, the default.
        Item {
            width: parent.width
            height: buttons.height + looks.paragraph_gap

            Row {
                id: buttons
                anchors { right: parent.right; bottom: parent.bottom }
                spacing: theme.dialog.button_spacing ?? 8

                Shell.PushButton {
                    id: laterButton
                    looks: theme.dialog
                    fonts: window.fonts
                    themeDirectory: window.themeDirectory
                    width: Math.max(wantedWidth, importButton.wantedWidth)
                    text: window.words.later ?? "Later"
                    onClicked: window.later()
                }
                Shell.PushButton {
                    id: importButton
                    looks: theme.dialog
                    fonts: window.fonts
                    themeDirectory: window.themeDirectory
                    isDefault: true
                    width: Math.max(wantedWidth, laterButton.wantedWidth)
                    text: window.words.import_again ?? "Import Again"
                    onClicked: window.importAgain()
                }
            }
        }
    }

    // Return is Import Again; Escape is Later.
    Shortcut {
        sequences: ["Return", "Enter"]
        onActivated: window.importAgain()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: window.later()
    }
}
