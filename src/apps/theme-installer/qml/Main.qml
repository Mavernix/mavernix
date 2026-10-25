// The Theme Installer's window: the steps in a
// column at the left, the current step's page at the right, Go Back and
// Continue along the bottom.
//
// Colours and sizes are the theme's [theme_installer] (buttons: [dialog]); words can
// be replaced in the config's [theme_installer.words]. `settings`, `job` and
// `givenInstaller` come from main.cpp.

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

    // The installer chosen, or "" while there is none, and which look it
    // makes: {look: id} or {problem: text}, worked out as soon as it is chosen.
    property string installer: givenInstaller
    property var identified: ({})
    onInstallerChanged: identified = installer !== "" ? job.identify(installer) : ({})
    Component.onCompleted: installerChanged()
    readonly property var chosenLook: job.looks.find(look => look.id === identified.look) ?? null

    // The step shown: an index into `steps`.
    property int step: givenInstaller !== "" ? 1 : 0
    readonly property var steps: [
        words.step_introduction ?? "Introduction",
        words.step_select ?? "Select Installer",
        words.step_import ?? "Import",
        words.step_summary ?? "Summary",
    ]
    readonly property bool importing: job.state === "running"

    title: words.window_title ?? "Theme Installer"
    width: looks.width
    height: looks.height
    minimumWidth: looks.width
    minimumHeight: looks.height
    visible: true
    color: "transparent"

    function startImport() {
        step = 2
        job.start(installer)
    }

    // When an import ends, its outcome is the summary.
    Connections {
        target: job
        function onStateChanged() {
            if (job.state !== "running" && job.state !== "idle") {
                window.step = 3
            }
        }
    }

    Shell.Fill {
        anchors.fill: parent
        fill: looks.background
        themeDirectory: window.themeDirectory
    }

    // --- The column of steps ---------------------------------------------------

    Item {
        id: sidebar
        width: looks.sidebar_width
        anchors { top: parent.top; bottom: parent.bottom; left: parent.left }

        Shell.Fill {
            anchors.fill: parent
            fill: looks.sidebar
            themeDirectory: window.themeDirectory
        }
        Rectangle {
            width: 1
            anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
            color: ThemeValues.color(looks.sidebar_line_color)
        }

        Repeater {
            model: window.steps
            delegate: Item {
                required property string modelData
                required property int index
                readonly property string progress:
                    index < window.step ? "done" : index === window.step ? "current" : "waiting"
                x: looks.step_inset
                y: looks.steps_top + index * looks.step_pitch
                width: sidebar.width - 2 * looks.step_inset
                height: looks.step_pitch

                Shell.Fill {
                    id: marker
                    width: looks.step_marker_size
                    height: looks.step_marker_size
                    anchors.verticalCenter: parent.verticalCenter
                    fill: looks["step_marker_" + parent.progress]
                    themeDirectory: window.themeDirectory
                }
                Text {
                    anchors { left: marker.right; leftMargin: looks.step_marker_gap; verticalCenter: parent.verticalCenter }
                    text: parent.modelData
                    font.family: fonts.theme_installer_text.family
                    font.pixelSize: fonts.theme_installer_text.size
                    font.bold: parent.progress === "current"
                    color: ThemeValues.color(parent.progress === "current"
                        ? looks.step_current_text_color : looks.step_text_color)
                }
            }
        }
    }

    // --- The page ----------------------------------------------------------------

    Item {
        id: page
        anchors {
            top: parent.top; bottom: buttonBar.top; right: parent.right; left: sidebar.right
            margins: looks.content_margin
        }

        Text {
            id: heading
            height: looks.heading_height
            width: parent.width
            text: [introduction.heading, selection.heading, progressPage.heading, summary.heading][window.step]
            font.family: fonts.theme_installer_heading.family
            font.pixelSize: fonts.theme_installer_heading.size
            font.bold: fonts.theme_installer_heading.bold ?? false
            color: ThemeValues.color(looks.text_color)
            elide: Text.ElideRight
        }

        Item {
            anchors { top: heading.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }

            IntroductionPage { id: introduction; anchors.fill: parent; visible: window.step === 0 }
            SelectPage { id: selection; anchors.fill: parent; visible: window.step === 1 }
            ProgressPage { id: progressPage; anchors.fill: parent; visible: window.step === 2 }
            SummaryPage { id: summary; anchors.fill: parent; visible: window.step === 3 }
        }
    }

    // --- The buttons ---------------------------------------------------------------

    Item {
        id: buttonBar
        height: looks.button_bar_height
        anchors { bottom: parent.bottom; left: sidebar.right; right: parent.right }

        Row {
            anchors { right: parent.right; rightMargin: looks.content_margin; verticalCenter: parent.verticalCenter }
            spacing: theme.dialog.button_spacing ?? 8

            Shell.PushButton {
                id: backButton
                opacity: enabled ? 1 : window.looks.disabled_button_opacity
                looks: theme.dialog
                fonts: window.fonts
                themeDirectory: window.themeDirectory
                width: Math.max(wantedWidth, continueButton.wantedWidth)
                text: window.step === 2 ? (words.stop ?? "Stop")
                    : window.step === 3 && job.state === "done" ? (words.close ?? "Close")
                    : (words.go_back ?? "Go Back")
                enabled: window.step === 2 ? window.importing : window.step > 0
                onClicked: {
                    if (window.step === 2) {
                        job.stop()
                    } else if (window.step === 3 && job.state === "done") {
                        window.close()
                    } else {
                        window.step = window.step === 3 ? 1 : window.step - 1
                    }
                }
            }
            Shell.PushButton {
                id: continueButton
                opacity: enabled ? 1 : window.looks.disabled_button_opacity
                looks: theme.dialog
                fonts: window.fonts
                themeDirectory: window.themeDirectory
                isDefault: true
                width: Math.max(wantedWidth, backButton.wantedWidth)
                text: window.step === 1 && window.chosenLook?.installed ? (words.reinstall_button ?? "Reinstall")
                    : window.step === 1 ? (words.install_button ?? "Install")
                    : window.step === 3 && job.state === "done" ? (words.use_look ?? "Use %1").arg(window.chosenLook?.name ?? "")
                    : window.step === 3 ? (words.close ?? "Close")
                    : (words.continue_button ?? "Continue")
                enabled: window.step === 0 ? job.missingPrograms.length === 0
                    : window.step === 1 ? window.chosenLook !== null
                    : window.step === 2 ? false
                    : true
                onClicked: {
                    if (window.step === 0) {
                        window.step = 1
                    } else if (window.step === 1) {
                        window.startImport()
                    } else if (job.state === "done") {
                        job.useDesktop(window.chosenLook.desktop)
                        window.close()
                    } else {
                        window.close()
                    }
                }
            }
        }
    }

    // Return is Continue; Escape stops an import or goes back.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: continueButton.enabled
        onActivated: continueButton.clicked()
    }
    Shortcut {
        sequence: "Escape"
        enabled: backButton.enabled
        onActivated: backButton.clicked()
    }
}
