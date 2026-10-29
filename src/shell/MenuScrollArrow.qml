// The strip at the top or bottom of a menu that scrolls, where there is
// more: the theme's arrow ([menu] scroll_arrow_up, scroll_arrow_down), over
// the menu's own background. Resting the pointer on it scrolls that way
// (MenuBox's arrowScrolling).

import QtQuick
import "ThemeValues.js" as ThemeValues

Item {
    id: strip

    // The MenuBox this belongs to, and the way it scrolls: -1 up, 1 down.
    required property var ownerMenu
    required property int direction

    width: ownerMenu.width
    height: ownerMenu.arrowStripHeight

    Text {
        anchors.centerIn: parent
        text: strip.direction < 0 ? (strip.ownerMenu.looks.scroll_arrow_up ?? "▲")
                                  : (strip.ownerMenu.looks.scroll_arrow_down ?? "▼")
        color: ThemeValues.color(strip.ownerMenu.looks.text_color)
        font.family: strip.ownerMenu.font.family
        font.pixelSize: strip.ownerMenu.looks.scroll_arrow_size ?? Math.round(strip.ownerMenu.font.size * 0.6)
    }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onContainsMouseChanged: strip.ownerMenu.arrowScrolling = containsMouse ? strip.direction : 0
        onVisibleChanged: {
            if (!visible && strip.ownerMenu.arrowScrolling === strip.direction) {
                strip.ownerMenu.arrowScrolling = 0
            }
        }
    }
}
