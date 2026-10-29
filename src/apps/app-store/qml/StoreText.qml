// A line of text in one of the App Store's fonts (Looks.font) and colours.

import QtQuick

Text {
    required property var looks
    // Which app_store_* font: "text", "small", "name", "heading", "title", "tab" or "button".
    property string kind: "text"

    readonly property var themeFont: looks.font(kind)

    color: looks.color(kind === "heading" ? "heading_color" : "text_color")
    font.family: themeFont.family
    font.pixelSize: themeFont.size
    font.bold: themeFont.bold ?? false
    elide: Text.ElideRight
}
