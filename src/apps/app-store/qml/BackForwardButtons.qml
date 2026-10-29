// The ◀ ▶ pair at the toolbar's left, one capsule with a line between its
// halves, as OS X's toolbars have it. A half that has nowhere to go is
// greyed and can't be pressed; a pressed half darkens on its own. While the
// window isn't the active one, the capsule is the lighter
// back_forward_inactive and both arrows are greyed.
//
// Both halves cut their part out of one capsule as wide as the pair, so a
// theme's picture of the capsule keeps its rounded ends. The arrows are the
// theme's pictures (back_glyph, forward_glyph and their _disabled forms), or
// plain triangles in arrow_color when it has none.

import QtQuick
import "../../../shell" as Shell

Item {
    id: pair

    required property var looks
    required property bool canGoBack
    required property bool canGoForward
    required property bool windowActive

    signal backChosen()
    signal forwardChosen()

    width: looks.value("back_forward_size")[0]
    height: looks.value("back_forward_size")[1]
    // Whole pixels: a half-pixel seam would blend the halves into a blurred column.
    readonly property int backWidth: Math.floor(width / 2)

    component Half: Item {
        id: half

        property bool isBack: true
        property bool usable: false

        width: isBack ? pair.backWidth : pair.width - pair.backWidth
        height: pair.height
        x: isBack ? 0 : pair.backWidth
        clip: true

        readonly property bool pressed: area.pressed && area.containsMouse && usable

        Shell.Fill {
            x: half.isBack ? 0 : -pair.backWidth
            width: pair.width
            height: pair.height
            themeDirectory: pair.looks.themeDirectory
            fill: {
                if (half.pressed) {
                    return pair.looks.value("back_forward_pressed")
                }
                return pair.looks.value(pair.windowActive ? "back_forward" : "back_forward_inactive")
            }
        }

        Image {
            id: glyph
            anchors.centerIn: parent
            visible: source != ""
            source: {
                const key = half.isBack ? "back_glyph" : "forward_glyph"
                const greyed = !half.usable || !pair.windowActive
                return pair.looks.picture(greyed ? key + "_disabled" : key) || pair.looks.picture(key)
            }
        }

        StoreText {
            anchors.centerIn: parent
            visible: !glyph.visible
            looks: pair.looks
            kind: "tab"
            color: pair.looks.color("arrow_color")
            opacity: half.usable ? 1 : 0.35
            text: half.isBack ? "◀" : "▶"
        }

        MouseArea {
            id: area
            anchors.fill: parent
            onClicked: {
                if (!half.usable) {
                    return
                }
                if (half.isBack) {
                    pair.backChosen()
                } else {
                    pair.forwardChosen()
                }
            }
        }
    }

    Half {
        isBack: true
        usable: pair.canGoBack
    }

    Half {
        isBack: false
        usable: pair.canGoForward
    }

    // The line between the halves stays off the capsule's own border.
    Shell.Fill {
        readonly property var inset: pair.looks.value("back_forward_separator_inset")
        x: pair.backWidth - Math.floor(width / 2)
        y: inset[0]
        width: pair.looks.value("back_forward_separator_width")
        height: parent.height - inset[0] - inset[1]
        themeDirectory: pair.looks.themeDirectory
        fill: pair.looks.value("back_forward_separator")
    }
}
