// Status item: the system tray, one icon per program in its own colours.
// Pressing opens the program's menu if it has one, else activates it.

import QtQuick
import Quickshell
import Quickshell.Services.SystemTray
import ".."

Row {
    id: tray

    required property var panel
    required property var itemRow

    readonly property var statusLooks: panel.looks.status ?? ({})

    // Programs whose menu another panel item shows (`tray_program` under
    // [status.NAME]); their icons are left out here.
    readonly property var takenOver: {
        const programs = []
        const statusWishes = panel.settings.config.status ?? ({})
        const onPanel = (panel.layout.left ?? []).concat(panel.layout.right ?? [])
        for (const name of onPanel) {
            const program = (statusWishes[name] ?? ({})).tray_program ?? ""
            if (program !== "") {
                programs.push(program)
            }
        }
        return programs
    }
    readonly property bool shown:
        SystemTray.items.values.some(program => !tray.takenOver.includes(program.id))

    spacing: statusLooks.spacing ?? itemRow.spacing
    height: itemRow.height

    Repeater {
        model: SystemTray.items

        StatusItem {
            id: icon

            required property var modelData

            panel: tray.panel
            itemRow: tray.itemRow
            visible: !tray.takenOver.includes(modelData.id)

            pictureTinted: false
            pictureSource: modelData.icon
            pictureHeight: tray.statusLooks.icon_size ?? 16

            TrayMenu {
                id: programMenu
                menu: icon.modelData.hasMenu ? icon.modelData.menu : null
            }
            menuItems: programMenu.items
            onChosen: id => programMenu.choose(id)
            onActivated: modelData.activate()
        }
    }
}
