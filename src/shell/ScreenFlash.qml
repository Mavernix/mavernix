// A short pale flash over the whole screen when a screenshot is taken.
// Theme: [screenshot_flash] color, opacity; [animation.screenshot_flash].
// The window is click-through and only shown while the flash runs.

import QtQuick
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: flashWindow

    required property var settings

    readonly property var looks: settings.theme.screenshot_flash ?? ({})
    readonly property var motion: (settings.theme.animation ?? ({})).screenshot_flash ?? ({})

    function flash() {
        clearing.restart()
    }

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    color: "transparent"
    mask: Region {}
    visible: clearing.running

    Rectangle {
        id: veil
        anchors.fill: parent
        color: ThemeValues.color(flashWindow.looks.color ?? "#ffffff")
        opacity: 0
    }

    NumberAnimation {
        id: clearing
        target: veil
        property: "opacity"
        from: flashWindow.looks.opacity ?? 0.6
        to: 0
        duration: flashWindow.motion.duration_ms ?? 250
        easing.type: ThemeValues.easing(flashWindow.motion.curve ?? "ease-out")
    }
}
