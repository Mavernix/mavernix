// A full-screen notice for when the desktop cannot start or its shell keeps
// failing. Runs NOTICE_COMMAND (arguments in NOTICE_ARGUMENT_1 ...), shows
// its last lines, and quits when it succeeds; on failure it stays until a
// key or click. Run under a bare compositor: qs -p .../session/notice

import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland

ShellRoot {
    id: notice

    readonly property string heading: Quickshell.env("NOTICE_HEADING") ?? ""
    readonly property string failedHeading: Quickshell.env("NOTICE_FAILED_HEADING") ?? ""
    readonly property string explanation: Quickshell.env("NOTICE_EXPLANATION") ?? ""
    readonly property string failedExplanation: Quickshell.env("NOTICE_FAILED_EXPLANATION")
        ?? "Press any key or the pointer's button to close this."

    readonly property var command: {
        const parts = [Quickshell.env("NOTICE_COMMAND") ?? "true"]
        for (let number = 1; Quickshell.env("NOTICE_ARGUMENT_" + number); number++) {
            parts.push(Quickshell.env("NOTICE_ARGUMENT_" + number))
        }
        return parts
    }

    // The command's most recent lines, and how it ended (-1 while running).
    readonly property int linesKept: 12
    property var lastLines: []
    property int exitCode: -1
    readonly property bool failed: exitCode > 0

    function takeLine(line) {
        lastLines = lastLines.concat([line]).slice(-linesKept)
    }

    Process {
        command: notice.command
        running: true
        stdout: SplitParser {
            onRead: line => notice.takeLine(line)
        }
        stderr: SplitParser {
            onRead: line => notice.takeLine(line)
        }
        onExited: code => {
            notice.exitCode = code
            if (code === 0) {
                Qt.quit()
            }
        }
    }

    PanelWindow {
        id: screenWindow
        anchors {
            left: true
            right: true
            top: true
            bottom: true
        }
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
        color: "#2b2d31"

        // No output scale is set yet, so size text from the screen height.
        readonly property real lineSize: height / 40

        Column {
            anchors.centerIn: parent
            width: parent.width * 0.7
            spacing: screenWindow.lineSize

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#f0f0f0"
                font.pixelSize: screenWindow.lineSize * 1.6
                font.bold: true
                text: notice.failed ? notice.failedHeading : notice.heading
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#c8c8c8"
                font.pixelSize: screenWindow.lineSize * 0.9
                text: notice.failed ? notice.failedExplanation : notice.explanation
            }
            Text {
                width: parent.width
                color: "#909090"
                font.family: "monospace"
                font.pixelSize: screenWindow.lineSize * 0.65
                elide: Text.ElideRight
                maximumLineCount: notice.linesKept
                wrapMode: Text.NoWrap
                text: notice.lastLines.join("\n")
            }
        }

        Item {
            anchors.fill: parent
            focus: true
            Keys.onPressed: {
                if (notice.failed) {
                    Qt.quit()
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (notice.failed) {
                        Qt.quit()
                    }
                }
            }
        }
    }
}
