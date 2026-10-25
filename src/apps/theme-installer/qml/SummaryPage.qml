// The last page: how the import ended. Done: the look is ready, and Use
// switches to it. Failed: why, in the back end's words. Stopped: nothing
// was changed.

import QtQuick
import "../../../shell" as Shell
import "../../../shell/ThemeValues.js" as ThemeValues

Item {
    readonly property string outcome: job.state
    readonly property string name: window.chosenLook?.name ?? ""
    readonly property string heading: outcome === "done" ? (window.words.done_heading ?? "%1 Is Ready").arg(name)
        : outcome === "stopped" ? (window.words.stopped_heading ?? "The Import Was Stopped")
        : (window.words.failed_heading ?? "The Import Did Not Finish")

    Shell.Fill {
        id: pane
        anchors.fill: parent
        fill: window.looks.pane
        themeDirectory: window.themeDirectory
    }

    Column {
        anchors { fill: pane; margins: window.looks.pane_padding }
        spacing: window.looks.paragraph_gap

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: outcome === "done" ? "✓" : outcome === "stopped" ? "■" : "!"
            font.pixelSize: window.looks.summary_mark_size
            font.bold: true
            color: ThemeValues.color(outcome === "done" ? window.looks.stage_done_color
                : outcome === "stopped" ? window.looks.secondary_text_color : window.looks.stage_failed_color)
        }
        BodyText {
            horizontalAlignment: Text.AlignHCenter
            text: outcome === "done"
                ? (window.words.done ?? "The %1 look is on this computer. Use it now, or choose it later in Settings, Theme.").arg(name)
                : outcome === "stopped"
                ? (window.words.stopped ?? "Nothing was changed.")
                : job.problem
        }
        BodyText {
            visible: outcome === "done" && job.notes.length > 0
            secondary: true
            horizontalAlignment: Text.AlignHCenter
            text: job.notes.join("\n")
        }
    }
}
