// The button that gets, updates and opens an app, as on OS X's App Store:
//
//   GET → (pressed) INSTALL → (pressed) a bar while it downloads → OPEN
//
// The first press only asks for a second one, so nothing is installed by a
// stray click. An installed app with a newer version on offer shows UPDATE.
// While the app waits for another to finish, or is being removed, the button
// says so and can't be pressed.

import QtQuick
import "../../../shell" as Shell

Item {
    id: button

    required property var looks
    required property var app
    // The smaller button of the grid and the lists, or the page's larger one.
    property bool small: false

    readonly property var job: store.jobs[app.name] ?? null
    readonly property bool installed: app.installed_version !== ""
    readonly property bool hasUpdate: installed && app.installed_version !== app.version
    // Set by the first press on GET; the next press installs.
    property bool confirming: false

    readonly property string label: {
        if (job !== null) {
            return looks.word(job.state === "downloading" ? "installing" : job.state)
        }
        if (!installed) {
            return looks.word(confirming ? "install" : "get")
        }
        return looks.word(hasUpdate ? "update" : "open")
    }
    readonly property bool showsProgress: job !== null && (job.state === "downloading" || job.state === "installing")

    implicitWidth: Math.max(text.implicitWidth + 2 * looks.value("button_padding"), height * 3)
    implicitHeight: looks.value(small ? "small_button_height" : "button_height")

    function press() {
        if (job !== null) {
            return
        }
        if (!installed && !confirming) {
            confirming = true
        } else if (!installed || hasUpdate) {
            confirming = false
            store.install(app.name)
        } else {
            store.open(app.name)
        }
    }

    Shell.Fill {
        anchors.fill: parent
        visible: !button.showsProgress
        themeDirectory: button.looks.themeDirectory
        fill: {
            const kind = button.confirming ? "button_confirm" : "button"
            return button.looks.value(kind + (area.pressed && area.containsMouse ? "_pressed" : ""))
        }
    }

    // While it installs: a bar that fills as the download arrives.
    Shell.Fill {
        anchors.fill: parent
        visible: button.showsProgress
        themeDirectory: button.looks.themeDirectory
        fill: button.looks.value("progress_track")
    }
    Shell.Fill {
        visible: button.showsProgress
        height: parent.height
        width: Math.max(height, parent.width * (button.job?.fraction ?? 0))
        themeDirectory: button.looks.themeDirectory
        fill: button.looks.value("progress_bar")
    }

    StoreText {
        id: text
        looks: button.looks
        kind: "button"
        anchors.centerIn: parent
        // The bar speaks for itself while it downloads.
        visible: !button.showsProgress
        text: button.label
        color: button.looks.color(button.confirming ? "button_confirm_text_color" : "button_text_color")
        opacity: button.job !== null ? 0.6 : 1
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: button.job === null
        onClicked: button.press()
    }
}
