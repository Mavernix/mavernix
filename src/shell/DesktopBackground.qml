// The desktop background: a fill (colour, gradient or picture) behind every
// window. On one screen it also shows the files of the Desktop folder.

import QtQuick
import Quickshell
import Quickshell.Wayland

PanelWindow {
    id: background

    required property var settings
    property var fill: settings.theme.desktop.background
    // The fill names a picture that cannot be read.
    signal pictureFailed()
    property bool showsFiles: false
    // The shell's UserChoices, which keep the order of the desktop's files.
    property var choices: null

    WlrLayershell.layer: WlrLayer.Background
    exclusionMode: ExclusionMode.Ignore
    anchors {
        top: true
        bottom: true
        left: true
        right: true
    }

    WlrLayershell.keyboardFocus: showsFiles ? WlrKeyboardFocus.OnDemand : WlrKeyboardFocus.None
    WlrLayershell.namespace: "desktop-background"

    // Shown while a picture is still loading, or if a theme gives no fill.
    color: "black"

    Fill {
        anchors.fill: parent
        fill: background.fill
        themeDirectory: background.settings.theme_directory
        onFailedChanged: {
            if (failed) {
                background.pictureFailed()
            }
        }
    }

    Loader {
        id: iconsLoader
        anchors.fill: parent
        active: background.showsFiles && background.choices !== null
        sourceComponent: DesktopIcons {
            settings: background.settings
            userChoices: background.choices
            screen: background.screen
        }
    }
}
