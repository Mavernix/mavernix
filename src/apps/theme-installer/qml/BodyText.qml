// A paragraph of a page: the theme's text type and colour, wrapped to the
// page's width. `secondary` makes it the quieter colour for notes.

import QtQuick
import "../../../shell/ThemeValues.js" as ThemeValues

Text {
    property bool secondary: false

    width: parent ? parent.width : 0
    wrapMode: Text.Wrap
    textFormat: Text.PlainText
    lineHeight: 1.15
    font.family: window.fonts.theme_installer_text.family
    font.pixelSize: window.fonts.theme_installer_text.size
    color: ThemeValues.color(secondary ? window.looks.secondary_text_color : window.looks.text_color)
}
