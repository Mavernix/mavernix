// The shadow a panel casts over the desktop and windows: the blurred edge of
// a dark bar lying where the panel is, drawn in a click-through window of
// its own just past the panel's edge.
//
// Theme: panel.shadow = { color, blur }.

import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: strip

    // The Panel window this shadow belongs to.
    required property var forPanel

    readonly property var looks: forPanel.looks.shadow
    // Beyond this distance from the panel nothing of the blur is left.
    readonly property real reach: looks.blur

    anchors {
        left: true
        right: true
        top: !forPanel.atBottom
        bottom: forPanel.atBottom
    }
    implicitHeight: reach

    // Placed past the panel by a margin, ignoring the room the panel reserves.
    exclusionMode: ExclusionMode.Ignore
    margins.top: forPanel.atBottom ? 0 : forPanel.height
    margins.bottom: forPanel.atBottom ? forPanel.height : 0
    WlrLayershell.layer: WlrLayer.Top

    color: "transparent"
    mask: Region {}

    RectangularShadow {
        width: parent.width
        height: strip.forPanel.height
        y: strip.forPanel.atBottom ? strip.reach : -height
        color: ThemeValues.color(strip.looks.color)
        blur: strip.looks.blur
        cached: false
    }
}
