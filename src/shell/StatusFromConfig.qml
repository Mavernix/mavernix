// A status item that needs no file of its own: the config describes it.
// It can show what a command prints, run again every so often; an icon; and
// a menu of commands. Any of the three alone will do.
//
//   [panel]
//   right = ["load", "clock", "session"]
//
//   [status.load]
//   command = ["sh", "-c", "cut -d' ' -f1 /proc/loadavg"]
//   every_seconds = 10                         # optional; without it, run once
//   icon = "utilities-system-monitor"          # optional
//   click_command = ["foot", "-e", "top"]      # optional: run when pressed
//
//   [status.session]
//   icon = "system-shutdown"
//   menu = [
//       { label = "Sleep", command = ["systemctl", "suspend"] },
//       { separator = true },
//       { label = "Shut Down…", command = ["systemctl", "poweroff"], confirm = { … } },
//   ]
//
// Only the first line of output is shown. Menu entries are written like
// those of the logo menu and may ask first; with a menu, click_command is
// not used. PanelItems.qml uses this for names with no file of their own.

import QtQuick
import Quickshell
import Quickshell.Io

StatusItem {
    id: item

    required property string name

    readonly property var wishes: ((panel.settings.config.status ?? ({}))[name]) ?? ({})

    iconName: wishes.icon ?? ""
    shown: text !== "" || iconName !== ""

    Process {
        id: command
        command: item.wishes.command ?? []
        running: item.wishes.command !== undefined
        stdout: StdioCollector {
            onStreamFinished: item.text = text.split("\n")[0].trim()
        }
    }

    Timer {
        running: (item.wishes.every_seconds ?? 0) > 0
        interval: (item.wishes.every_seconds ?? 1) * 1000
        repeat: true
        onTriggered: command.running = true
    }

    readonly property var entries: wishes.menu ?? []
    menuItems: entries.map((entry, index) => ({
        id: index,
        label: entry.label ?? "",
        enabled: true,
        separator: entry.separator ?? false,
        checkable: false, checked: false, shortcut: [], children: []
    }))
    onChosen: id => panel.runEntry(entries[id])

    onActivated: {
        if (wishes.click_command !== undefined) {
            Quickshell.execDetached(wishes.click_command)
        }
    }
}
