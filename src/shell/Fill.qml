// Draws a theme "fill" behind whatever it is placed in: a plain colour, a
// top-to-bottom gradient, or a picture that stretches (docs/themes/,
// "Fills").
//
// Usage:  Fill { anchors.fill: parent; fill: theme.panel.background; themeDirectory: ... }

import QtQuick
import "ThemeValues.js" as ThemeValues

Item {
    id: root

    // The fill as written in the theme, e.g. { color: "#e0e0e0" }.
    property var fill: ({})
    property string themeDirectory: ""

    readonly property bool isColor: fill.color !== undefined
    readonly property bool isGradient: fill.gradient !== undefined
    readonly property bool isImage: fill.image !== undefined

    Rectangle {
        anchors.fill: parent
        visible: root.isColor
        color: root.isColor ? ThemeValues.color(root.fill.color) : "transparent"
        radius: root.fill.radius ?? 0
        border.width: root.fill.border?.width ?? 0
        border.color: root.fill.border !== undefined ? ThemeValues.color(root.fill.border.color) : "transparent"
    }

    // Gradient. QML wants its colour stops as objects, and a theme can have
    // any number of them, so they are created to match when the fill is set.
    Rectangle {
        id: gradientBox
        anchors.fill: parent
        visible: root.isGradient
        radius: root.fill.radius ?? 0
        border.width: root.fill.border?.width ?? 0
        border.color: root.fill.border !== undefined ? ThemeValues.color(root.fill.border.color) : "transparent"
        gradient: Gradient {
            id: gradientStops
        }

        Component {
            id: stopTemplate
            GradientStop {}
        }

        function rebuildStops() {
            if (!root.isGradient) {
                gradientStops.stops = []
                return
            }

            const positions = ThemeValues.gradientPositions(root.fill)
            const stops = []
            for (let index = 0; index < root.fill.gradient.length; index++) {
                stops.push(stopTemplate.createObject(gradientStops, {
                    position: positions[index],
                    color: ThemeValues.color(root.fill.gradient[index])
                }))
            }
            gradientStops.stops = stops
        }

        Component.onCompleted: rebuildStops()
        Connections {
            target: root
            function onFillChanged() { gradientBox.rebuildStops() }
        }
    }

    // fit = "cover": scaled to fill both directions, the overhang cut off.
    readonly property bool coversArea: isImage && fill.fit === "cover"

    // Relative to the theme, unless the fill gives a full path.
    readonly property string imageAddress: {
        if (!isImage) {
            return ""
        }
        const inTheme = !fill.image.startsWith("/")
        return "file://" + (inTheme ? themeDirectory + "/" : "") + fill.image
    }

    readonly property bool failed: coverPicture.status === Image.Error

    Image {
        id: coverPicture
        anchors.fill: parent
        visible: root.coversArea
        source: root.coversArea ? root.imageAddress : ""
        fillMode: Image.PreserveAspectCrop
        // A wallpaper file can be far larger than the screen; decode it at
        // the size it is shown at instead of holding all of it in memory.
        sourceSize.width: width
        sourceSize.height: height
        asynchronous: true
    }

    // A stretched picture: `slice` edges keep their size. Qt picks an
    // "@2x" file by itself on double-density displays.
    BorderImage {
        anchors.fill: parent
        visible: root.isImage && !root.coversArea
        source: visible ? root.imageAddress : ""

        // `slice` in a theme is top, right, bottom, left.
        readonly property var slice: root.fill.slice ?? [0, 0, 0, 0]
        border.top: slice[0]
        border.right: slice[1]
        border.bottom: slice[2]
        border.left: slice[3]
    }
}
