// A folder's contents as a fan rising from its dock tile, each file an icon
// with its name beside it, then one entry that opens the folder (and says
// how many files are not shown). Looks: the theme's [dock.folder.fan]
// (docs/themes/, "Folders in the dock"). (0, 0) is the middle of the first
// icon's bottom edge.

import QtQuick
import Quickshell
import "ThemeValues.js" as ThemeValues

Item {
    id: fan

    IconSource {
        id: themeIcons
    }

    required property var settings

    // { name, path, icon, genericIcon } each, from FolderContents.
    property var entries: []
    // The last entry's name when all files show, and when some do not.
    property string openLabel: ""
    property string moreLabel: "%1"
    // How far the fan has risen out of its tile, from 0 to 1.
    property real shown: 1

    signal fileChosen(string path)
    signal folderChosen()

    readonly property var looks: settings.theme.dock.folder?.fan ?? ({})
    readonly property var labelLooks: looks.label ?? settings.theme.dock.label
    readonly property var font: settings.theme.fonts.dock_folder ?? settings.theme.fonts.interface
    readonly property string themeDirectory: settings.theme_directory

    readonly property real iconSize: looks.icon_size ?? 48
    readonly property real pitch: looks.pitch ?? (iconSize + 4)
    // In degrees; 0 gives a straight column.
    readonly property real turnPerEntry: looks.turn_per_entry ?? 0

    // The folder entry included.
    readonly property int mostEntries: looks.most_entries ?? 8
    readonly property int shownFiles: Math.min(entries.length, mostEntries - 1)
    readonly property int hiddenFiles: entries.length - shownFiles

    // The middle and turn of the entry at a place along the fan (fractional
    // while it rises). The entries lie on an arc, like spokes of a circle.
    function placeAt(position) {
        const turn = position * turnPerEntry
        if (turnPerEntry === 0) {
            return { x: 0, y: -(position + 0.5) * pitch, turn: 0 }
        }
        const step = turnPerEntry * Math.PI / 180
        const radius = pitch / Math.sin(step)
        return {
            x: radius * (1 - Math.cos(position * step)),
            y: -pitch / 2 - radius * Math.sin(position * step),
            turn: turn
        }
    }

    Repeater {
        model: fan.shownFiles + 1

        Item {
            id: entry
            required property int index
            readonly property var file: index < fan.shownFiles ? fan.entries[index] : null
            readonly property var place: fan.placeAt(index * fan.shown)

            // A point at the entry's middle; icon and name turn around it.
            x: place.x
            y: place.y
            rotation: place.turn
            opacity: fan.shown

            readonly property bool hasOwnPicture: file === null && fan.looks.open_image !== undefined
            readonly property real pictureSize:
                hasOwnPicture ? (fan.looks.open_image_size ?? fan.iconSize) : fan.iconSize
            Image {
                x: -width / 2
                y: -height / 2
                width: entry.pictureSize
                height: entry.pictureSize
                sourceSize: Qt.size(width, height)
                source: {
                    if (entry.hasOwnPicture) {
                        return "file://" + fan.themeDirectory + "/" + fan.looks.open_image
                    }
                    if (entry.file === null) {
                        return themeIcons.path("folder-open")
                    }
                    const own = themeIcons.find(entry.file.icon)
                    return own !== "" ? own
                                      : themeIcons.path(entry.file.genericIcon, "text-x-generic")
                }
                opacity: press.pressed ? 0.6 : 1
            }

            Rectangle {
                id: bubble
                width: name.width + 2 * (fan.labelLooks.padding ?? 8)
                height: fan.labelLooks.height ?? 20
                radius: height / 2
                color: ThemeValues.color(fan.labelLooks.background)
                x: -fan.iconSize / 2 - (fan.looks.label_gap ?? 4) - width
                y: -height / 2 - (fan.looks.label_lift ?? 0)

                Text {
                    id: name
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, fan.looks.label_width ?? 240)
                    elide: Text.ElideMiddle
                    text: {
                        if (entry.file !== null) {
                            return entry.file.name
                        }
                        return fan.hiddenFiles > 0 ? fan.moreLabel.arg(fan.hiddenFiles) : fan.openLabel
                    }
                    color: ThemeValues.color(fan.labelLooks.text_color)
                    font.family: fan.font.family
                    font.pixelSize: fan.font.size
                    font.bold: fan.font.bold ?? false
                }
            }

            MouseArea {
                id: press
                x: bubble.x
                y: -fan.iconSize / 2
                width: fan.iconSize / 2 - bubble.x
                height: fan.iconSize
                onClicked: {
                    if (entry.file !== null) {
                        fan.fileChosen(entry.file.path)
                    } else {
                        fan.folderChosen()
                    }
                }
            }
        }
    }
}
