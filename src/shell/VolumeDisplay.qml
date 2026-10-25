// The on-screen loudness display shown briefly after a volume key: a
// loudspeaker picture and a row of segments. `show()` brings it up; it
// leaves by itself. Theme: [volume_display], [animation.volume_display];
// a theme without the table has no display.

import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: display

    IconSource {
        id: themeIcons
    }

    required property var settings
    required property var outputVolume

    readonly property var looks: settings.theme.volume_display ?? null
    readonly property var segmentLooks: looks?.segments ?? ({})
    readonly property var motion:
        (settings.theme.animation ?? ({})).volume_display ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    property real shown: 0

    function show() {
        if (looks === null) {
            return
        }
        leaving.stop()
        shown = 1
        staying.restart()
    }

    Timer {
        id: staying
        interval: display.motion.stay_ms ?? 1000
        onTriggered: leaving.start()
    }
    NumberAnimation {
        id: leaving
        target: display
        property: "shown"
        to: 0
        duration: display.motion.fade_out_ms ?? 300
    }

    visible: shown > 0

    // Bottom middle, above everything, click-through.
    anchors.bottom: true
    margins.bottom: looks?.bottom ?? 100
    implicitWidth: looks?.width ?? 200
    implicitHeight: looks?.height ?? 200
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    color: "transparent"
    mask: Region {}

    Item {
        anchors.fill: parent
        opacity: display.shown

        Fill {
            anchors.fill: parent
            fill: display.looks?.background ?? ({ color: "#00000080", radius: 12 })
            themeDirectory: display.themeDirectory
        }

        // The loudspeaker: the theme's full-size picture, or a tinted icon.
        readonly property bool muted: display.outputVolume.muted || display.outputVolume.loudness <= 0
        readonly property var pictures: display.looks?.pictures ?? null
        Image {
            visible: parent.pictures !== null
            anchors.fill: parent
            source: parent.pictures !== null
                ? "file://" + display.themeDirectory + "/" + (parent.muted ? parent.pictures.muted : parent.pictures.volume)
                : ""
        }
        Image {
            id: icon
            visible: parent.pictures === null
            readonly property real size: display.looks?.icon_size ?? 64
            anchors.horizontalCenter: parent.horizontalCenter
            y: display.looks?.icon_top ?? 30
            width: size
            height: size
            sourceSize: Qt.size(size, size)
            source: parent.pictures === null
                ? themeIcons.find(parent.muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic")
                : ""
            // Tinted like the panel's icons (see PanelTitle.qml).
            layer.enabled: true
            layer.effect: MultiEffect {
                brightness: 1.0
                colorization: 1.0
                colorizationColor: ThemeValues.color(display.looks?.icon_color ?? "#ffffff")
            }
        }

        readonly property int segmentCount: display.segmentLooks.count ?? 16
        readonly property int filled: muted ? 0 : Math.round(display.outputVolume.loudness * segmentCount)
        Repeater {
            model: parent.segmentCount
            Rectangle {
                required property int index
                x: (display.segmentLooks.left ?? 20) + index * (display.segmentLooks.pitch ?? 10)
                y: display.segmentLooks.top ?? 160
                width: display.segmentLooks.width ?? 8
                height: display.segmentLooks.height ?? 8
                color: ThemeValues.color(index < parent.filled
                    ? (display.segmentLooks.color ?? "#ffffff")
                    : (display.segmentLooks.empty_color ?? "#ffffff30"))
            }
        }
    }
}
