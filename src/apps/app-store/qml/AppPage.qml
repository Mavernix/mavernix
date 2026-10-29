// One app's page, laid out like OS X's store: the icon and its button on the
// left, the name, description and what's new in the middle, an Information
// column on the right, and the screenshots below.

import QtQuick

Flickable {
    id: page

    required property var looks
    required property var app

    readonly property real margin: looks.value("side_margin")
    readonly property real leftWidth: looks.value("page_icon_size") + margin
    readonly property real informationWidth: looks.value("information_width")
    readonly property real middleWidth: width - leftWidth - informationWidth - 3 * margin
    readonly property string failure: store.failures[app.name] ?? ""

    contentHeight: Math.max(middle.y + middle.height, information.y + information.height) + margin
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // "45.4 MB", the way OS X counts (1000, not 1024).
    function sizeText(bytes) {
        return bytes >= 1e9 ? (bytes / 1e9).toFixed(1) + " GB" : (bytes / 1e6).toFixed(1) + " MB"
    }

    Column {
        id: left
        x: page.margin
        y: page.margin
        width: page.looks.value("page_icon_size")
        spacing: 16

        Image {
            width: parent.width
            height: width
            sourceSize: Qt.size(width * 2, height * 2)
            source: "file://" + page.app.icon
            fillMode: Image.PreserveAspectFit
        }

        StoreButton {
            anchors.horizontalCenter: parent.horizontalCenter
            looks: page.looks
            app: page.app
        }

        StoreText {
            width: parent.width
            visible: page.failure !== ""
            looks: page.looks
            kind: "small"
            color: page.looks.color("failure_text_color")
            text: page.failure
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    Column {
        id: middle
        x: page.leftWidth + 2 * page.margin
        y: page.margin
        width: page.middleWidth
        spacing: 10

        StoreText {
            width: parent.width
            looks: page.looks
            kind: "title"
            text: page.app.title
        }

        StoreText {
            width: parent.width
            looks: page.looks
            text: page.app.description || page.app.summary
            wrapMode: Text.Wrap
        }

        StoreText {
            width: parent.width
            topPadding: 8
            visible: page.app.whats_new !== ""
            looks: page.looks
            kind: "heading"
            text: page.looks.word("whats_new", { version: page.app.version })
        }

        StoreText {
            width: parent.width
            visible: page.app.whats_new !== ""
            looks: page.looks
            text: page.app.whats_new
            textFormat: Text.MarkdownText
            wrapMode: Text.Wrap
        }

        Repeater {
            model: page.app.screenshots

            Image {
                required property string modelData
                // Screenshots start under the icon column too, like OS X's.
                x: -(page.leftWidth + page.margin)
                width: page.leftWidth + page.margin + page.middleWidth
                height: Math.min(page.looks.value("screenshot_height"), width * implicitHeight / Math.max(1, implicitWidth))
                source: "file://" + modelData
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignLeft
            }
        }
    }

    Column {
        id: information
        x: page.width - page.informationWidth - page.margin
        y: page.margin
        width: page.informationWidth
        spacing: 4

        SectionHeading {
            width: parent.width
            looks: page.looks
            text: page.looks.word("information")
        }

        Repeater {
            model: [
                ["category", page.app.category],
                ["released", page.app.released],
                ["version", page.app.version],
                ["size", page.sizeText(page.app.bundle_size)],
                ["developer", page.app.developer],
                ["license", page.app.license],
            ].filter(fact => fact[1] !== "")

            Row {
                required property var modelData
                width: information.width
                spacing: 4

                StoreText {
                    looks: page.looks
                    kind: "small"
                    color: page.looks.color("secondary_text_color")
                    text: page.looks.word(modelData[0]) + ":"
                }

                StoreText {
                    width: parent.width - x
                    looks: page.looks
                    kind: "small"
                    text: modelData[1]
                    wrapMode: Text.Wrap
                }
            }
        }

        StoreText {
            topPadding: 8
            looks: page.looks
            kind: "small"
            color: page.looks.color("secondary_text_color")
            text: page.looks.word("website") + " ›"
            font.underline: websiteArea.containsMouse

            MouseArea {
                id: websiteArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally(page.app.repository)
            }
        }
    }
}
