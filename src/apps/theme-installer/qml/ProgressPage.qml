// The third page: the import's stages, a bar for the whole import, the
// current detail and the time left.

import QtQuick
import "../../../shell" as Shell
import "../../../shell/ThemeValues.js" as ThemeValues

Item {
    readonly property string heading: window.words.import_heading ?? "Importing"

    // "About 3 minutes left", "Less than a minute left", or "" while there
    // is no estimate yet.
    function timeLeft(seconds) {
        if (seconds < 0) {
            return ""
        }
        if (seconds < 60) {
            return window.words.less_than_a_minute ?? "Less than a minute left"
        }
        const minutes = Math.round(seconds / 60)
        return (minutes === 1 ? (window.words.one_minute ?? "About a minute left")
            : (window.words.minutes ?? "About %1 minutes left")).replace("%1", minutes)
    }

    Shell.Fill {
        id: pane
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: detailText.top
                  bottomMargin: window.looks.paragraph_gap }
        fill: window.looks.pane
        themeDirectory: window.themeDirectory

        Column {
            anchors { fill: parent; margins: window.looks.pane_padding }

            Repeater {
                model: job.stages
                delegate: Item {
                    required property var modelData
                    readonly property string progress: modelData.state
                    width: parent.width
                    height: window.looks.stage_pitch

                    // ✓ done, ▸ running, ○ waiting: drawn as text in the
                    // stage's colour.
                    Text {
                        id: mark
                        width: window.looks.stage_pitch
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.progress === "done" ? "✓" : parent.progress === "running" ? "▸" : "○"
                        font.pixelSize: window.fonts.theme_installer_text.size
                        color: ThemeValues.color(window.looks["stage_" + parent.progress + "_color"])
                    }
                    Text {
                        anchors { left: mark.right; verticalCenter: parent.verticalCenter }
                        text: parent.modelData.label
                        font.family: window.fonts.theme_installer_text.family
                        font.pixelSize: window.fonts.theme_installer_text.size
                        font.bold: parent.progress === "running"
                        color: ThemeValues.color(parent.progress === "waiting"
                            ? window.looks.secondary_text_color : window.looks.text_color)
                    }
                }
            }
        }
    }

    // What the import is doing just now, the bar for the whole import, and
    // the time left.
    BodyText {
        id: detailText
        anchors { left: parent.left; right: parent.right; bottom: bar.top; bottomMargin: window.looks.paragraph_gap / 2 }
        elide: Text.ElideRight
        wrapMode: Text.NoWrap
        text: job.detail
    }

    Item {
        id: bar
        anchors { left: parent.left; right: parent.right; bottom: estimate.top; bottomMargin: window.looks.paragraph_gap / 2 }
        height: window.looks.progress_height

        Shell.Fill {
            anchors.fill: parent
            fill: window.looks.progress_track
            themeDirectory: window.themeDirectory
        }
        Shell.Fill {
            width: Math.max(height, parent.width * job.fraction)
            height: parent.height
            visible: job.fraction > 0
            fill: window.looks.progress_fill
            themeDirectory: window.themeDirectory
        }
    }

    BodyText {
        id: estimate
        anchors { left: parent.left; bottom: parent.bottom }
        wrapMode: Text.NoWrap
        secondary: true
        // A line's height even before there is an estimate, so that nothing moves.
        text: timeLeft(job.secondsLeft) || " "
    }
}
