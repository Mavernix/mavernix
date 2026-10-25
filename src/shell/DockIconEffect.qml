// An icon drawn as an effect: blurred, tinted, flipped or with contrast and
// brightness changed. The dock uses it for shadows and reflections. The
// picture is stretched to this item's size.
//
// The effect is drawn at a fixed size (`drawnWidth`) and scaled: MultiEffect
// rebuilds its buffers on every size change, which leaves stale rectangles
// around icons while the dock is resized.

import QtQuick
import QtQuick.Effects
import "ThemeValues.js" as ThemeValues

Item {
    id: effect

    property string iconSource: ""
    // The largest width this item gets.
    required property real drawnWidth
    // In logical pixels of this item.
    property real blurRadius: 0
    // A theme colour to fill the icon's shape with, or "" for none.
    property string tint: ""
    property bool upsideDown: false
    property real contrast: 1
    property real brightness: 0

    // Pixels across.
    readonly property int largestBlur: 32

    readonly property bool tinted: tint !== ""
    readonly property color tintColor: ThemeValues.color(tinted ? tint : "#000000")

    readonly property real shrink: width > 0 ? width / drawnWidth : 1

    Item {
        id: drawn
        width: effect.drawnWidth
        height: effect.width > 0 ? effect.drawnWidth * effect.height / effect.width : effect.drawnWidth
        transform: Scale {
            xScale: effect.shrink
            yScale: effect.shrink
        }

        Image {
            id: picture
            anchors.fill: parent
            visible: false
            source: effect.iconSource
            // A fixed square, so magnification does not reload the picture
            // at every step.
            sourceSize.width: effect.drawnWidth * Screen.devicePixelRatio
            sourceSize.height: effect.drawnWidth * Screen.devicePixelRatio
        }

        MultiEffect {
            anchors.fill: parent
            source: picture
            visible: effect.iconSource !== ""

            blurEnabled: effect.blurRadius > 0
            // blurMax stays fixed because changing it rebuilds the effect;
            // the radius goes through `blur` (0..1), corrected for the scaling.
            blurMax: effect.largestBlur
            blur: Math.min(1, (effect.blurRadius / effect.shrink) * 2 / effect.largestBlur)

            colorization: effect.tinted ? 1 : 0
            colorizationColor: Qt.rgba(effect.tintColor.r, effect.tintColor.g, effect.tintColor.b, 1)
            opacity: effect.tinted ? effect.tintColor.a : 1

            // MultiEffect's contrast is 0 for "unchanged" where the theme's is 1.
            contrast: effect.contrast - 1
            brightness: effect.brightness

            transform: Scale {
                origin.y: drawn.height / 2
                yScale: effect.upsideDown ? -1 : 1
            }
        }
    }
}
