// The first page: the looks the installer can make and which are installed,
// and anything missing on this machine.

import QtQuick
import "../../../shell" as Shell
import "../../../shell/ThemeValues.js" as ThemeValues

Item {
    readonly property string heading: window.words.introduction_heading ?? "Welcome to the Theme Installer"
    readonly property var missing: job.missingPrograms

    Shell.Fill {
        id: pane
        anchors.fill: parent
        fill: window.looks.pane
        themeDirectory: window.themeDirectory
    }

    Column {
        anchors { fill: pane; margins: window.looks.pane_padding }
        spacing: window.looks.paragraph_gap

        BodyText {
            text: window.words.introduction ??
                "Each look below is built on this computer from an installer you provide. "
                + "Nothing is downloaded."
        }

        // Each look: its name and whether it is installed, what it is, what it is made from.
        Repeater {
            model: job.looks
            delegate: Column {
                required property var modelData
                width: parent.width
                spacing: 2

                Row {
                    spacing: window.looks.step_marker_gap
                    Text {
                        text: modelData.name
                        font.family: window.fonts.theme_installer_text.family
                        font.pixelSize: window.fonts.theme_installer_text.size
                        font.bold: true
                        color: ThemeValues.color(window.looks.text_color)
                    }
                    Text {
                        text: modelData.installed ? "✓ " + (window.words.installed ?? "Installed")
                                                  : (window.words.not_installed ?? "Not installed")
                        font.family: window.fonts.theme_installer_text.family
                        font.pixelSize: window.fonts.theme_installer_text.size
                        color: ThemeValues.color(modelData.installed ? window.looks.stage_done_color
                                                                     : window.looks.secondary_text_color)
                    }
                }
                BodyText {
                    text: modelData.description + " " + (window.words.made_from ?? "Made from %1.").arg(modelData.source)
                }
                BodyText {
                    visible: modelData.licence_note !== ""
                    secondary: true
                    text: modelData.licence_note
                }
            }
        }

        // What has to be installed first, if anything.
        Shell.Fill {
            visible: missing.length > 0 || !job.sandbox
            width: parent.width
            height: noteText.implicitHeight + 2 * window.looks.pane_padding
            fill: window.looks.note
            themeDirectory: window.themeDirectory

            BodyText {
                id: noteText
                x: window.looks.pane_padding
                y: window.looks.pane_padding
                width: parent.width - 2 * window.looks.pane_padding
                text: {
                    if (missing.length > 0) {
                        const packages = missing.map(entry => entry.package)
                        const programs = missing.map(entry => `${entry.program} (${entry.purpose})`)
                        return (window.words.missing ??
                                "Before you can continue, install what the import needs with your system's package manager:")
                            + "\n" + programs.join("\n")
                            + "\n\n" + (window.words.on_arch ?? "On Arch Linux:") + " sudo pacman -S --needed " + packages.join(" ")
                    }
                    if (job.sandboxInstalled) {
                        return window.words.sandbox_unusable ??
                            "bubblewrap cannot make a sandbox here (it needs unprivileged user namespaces), "
                            + "so the installer will be read without one."
                    }
                    return (window.words.no_sandbox ??
                        "bubblewrap is not installed, so the installer would be read without a sandbox. "
                        + "Installing it first is recommended.")
                        + "\n\n" + (window.words.on_arch ?? "On Arch Linux:") + " sudo pacman -S --needed " + job.sandboxPackage
                }
            }
        }
    }
}
