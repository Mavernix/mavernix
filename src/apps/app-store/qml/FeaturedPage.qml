// The first page: every app on offer, as a grid of tiles under a heading,
// and the extensions as a grid of their own under another. Search results
// are the same page with fewer of each.

import QtQuick

Flickable {
    id: page

    required property var looks
    required property var apps
    required property string heading
    // Over the extensions' grid.
    required property string extensionsHeading

    signal appChosen(string name)

    readonly property var programs: apps.filter(app => app.kind !== "extension")
    readonly property var extensions: apps.filter(app => app.kind === "extension")

    contentHeight: content.height + 2 * looks.value("side_margin")
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    component TileGrid: Flow {
        required property var tiles
        width: parent.width
        spacing: 8

        Repeater {
            model: parent.tiles

            AppTile {
                required property var modelData
                looks: page.looks
                app: modelData
                onChosen: page.appChosen(modelData.name)
            }
        }
    }

    Column {
        id: content
        x: page.looks.value("side_margin")
        y: page.looks.value("side_margin")
        width: page.width - 2 * x
        spacing: 16

        SectionHeading {
            width: parent.width
            looks: page.looks
            text: page.heading
        }

        TileGrid {
            visible: page.programs.length > 0
            tiles: page.programs
        }

        SectionHeading {
            visible: page.extensions.length > 0
            width: parent.width
            looks: page.looks
            text: page.extensionsHeading
        }

        TileGrid {
            visible: page.extensions.length > 0
            tiles: page.extensions
        }
    }
}
