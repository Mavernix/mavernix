// Text in a folder's grid in the dock: the theme's [fonts].dock_folder, in
// the names' colour ([dock.folder.grid] label_color) unless set otherwise.

import QtQuick
import "ThemeValues.js" as ThemeValues

Text {
    // The FolderGrid this is in, for its looks and font.
    required property var folderGrid

    color: ThemeValues.color(folderGrid.looks.label_color ?? "#ffffff")
    font.family: folderGrid.font.family
    font.pixelSize: folderGrid.font.size
    font.bold: folderGrid.font.bold ?? false
}
