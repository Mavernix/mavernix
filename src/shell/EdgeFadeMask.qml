// A mask for a layer that fades out at its top and bottom: white where the
// layer shows, turning clear over `fadeLength` at each end. Used through
// MultiEffect's maskSource; it is not drawn itself.

import QtQuick

Item {
    property real fadeLength: 0

    visible: false
    layer.enabled: true

    Rectangle {
        width: parent.width
        height: parent.fadeLength
        gradient: Gradient {
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 1; color: "white" }
        }
    }
    Rectangle {
        y: parent.fadeLength
        width: parent.width
        height: parent.height - 2 * parent.fadeLength
        color: "white"
    }
    Rectangle {
        y: parent.height - parent.fadeLength
        width: parent.width
        height: parent.fadeLength
        gradient: Gradient {
            GradientStop { position: 0; color: "white" }
            GradientStop { position: 1; color: "transparent" }
        }
    }
}
