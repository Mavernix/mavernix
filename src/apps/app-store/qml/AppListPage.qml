// A page that lists apps, one row each: the Installed page and the Updates
// page are both this, with other apps and words.

import QtQuick

Flickable {
    id: page

    required property var looks
    required property var apps
    property string heading: ""
    property string emptyText: ""
    // Whether rows offer Remove (the Installed page).
    property bool removable: false
    // Whether the heading has a button that updates every app listed.
    property bool offersUpdateAll: false

    signal appChosen(string name)

    contentHeight: content.height + 2 * looks.value("side_margin")
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    Column {
        id: content
        x: page.looks.value("side_margin")
        y: page.looks.value("side_margin")
        width: page.width - 2 * x

        SectionHeading {
            width: parent.width
            looks: page.looks
            text: page.heading

            UpdateAllButton {
                visible: page.offersUpdateAll && page.apps.length > 0
                looks: page.looks
                apps: page.apps
            }
        }

        StoreText {
            visible: page.apps.length === 0
            width: parent.width
            topPadding: 40
            horizontalAlignment: Text.AlignHCenter
            looks: page.looks
            color: page.looks.color("secondary_text_color")
            text: page.emptyText
        }

        Repeater {
            model: page.apps

            AppRow {
                required property var modelData
                width: content.width
                looks: page.looks
                app: modelData
                removable: page.removable
                onChosen: page.appChosen(modelData.name)
            }
        }
    }
}
