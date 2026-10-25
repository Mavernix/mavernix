// A bar for work whose length is not known: a piece of the progress fill
// sliding to and fro along the track. Drawn from the same theme values as
// the import's bar.

import QtQuick
import "../../../shell" as Shell

Item {
    // A third of the track is filled at a time.
    readonly property real pieceShare: 1 / 3
    readonly property int sweepMs: 1100

    width: parent ? parent.width : 0
    height: window.looks.progress_height

    Shell.Fill {
        anchors.fill: parent
        fill: window.looks.progress_track
        themeDirectory: window.themeDirectory
    }
    Shell.Fill {
        id: piece
        width: Math.max(height, parent.width * parent.pieceShare)
        height: parent.height
        fill: window.looks.progress_fill
        themeDirectory: window.themeDirectory

        SequentialAnimation on x {
            running: piece.visible
            loops: Animation.Infinite
            NumberAnimation {
                from: 0
                to: piece.parent.width - piece.width
                duration: sweepMs
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                from: piece.parent.width - piece.width
                to: 0
                duration: sweepMs
                easing.type: Easing.InOutQuad
            }
        }
    }
}
