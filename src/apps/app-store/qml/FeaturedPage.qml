// The first page: every app on offer, as a grid of tiles under a heading.
// Search results are the same page with fewer apps.

import QtQuick

Flickable {
    id: page

    required property var looks
    required property var apps
    required property string heading

    signal appChosen(string name)

    contentHeight: content.height + 2 * looks.value("side_margin")
    clip: true
    boundsBehavior: Flickable.StopAtBounds

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

        Flow {
            width: parent.width
            spacing: 8

            Repeater {
                model: page.apps

                AppTile {
                    required property var modelData
                    looks: page.looks
                    app: modelData
                    onChosen: page.appChosen(modelData.name)
                }
            }
        }
    }
}
