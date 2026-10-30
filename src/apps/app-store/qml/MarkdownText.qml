// Text written in Markdown, such as an app's description or a release's
// notes: wrapped to its width, with links that open in the browser.

import QtQuick

StoreText {
    textFormat: Text.MarkdownText
    wrapMode: Text.Wrap
    onLinkActivated: link => Qt.openUrlExternally(link)

    HoverHandler {
        cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
