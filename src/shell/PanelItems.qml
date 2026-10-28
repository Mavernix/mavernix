// A row of panel items, built from a list of names in the config:
//   "logo", "app_name", "menus", "clock", or the name of a status item file
//   NAME.qml from panel.item_folders, an extension or status/
//   (docs/panel-items.md). Unknown names are shown in brackets so that a
//   typo in the config is visible.

import QtQuick
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

Row {
    id: row

    required property var names
    required property var panel

    spacing: panel.looks.item_spacing

    // Status items stand at their own spacing (panel.status.spacing). The row
    // adds its usual spacing after every item, so each is resized by the
    // difference, except the last, whose picture must end where the row ends.
    readonly property real statusSpacing: (panel.looks.status ?? ({})).spacing ?? spacing
    readonly property int lastShownIndex: {
        for (let index = items.count - 1; index >= 0; index--) {
            const loader = items.itemAt(index)
            if (loader !== null && loader.visible) {
                return index
            }
        }
        return -1
    }
    function statusGapAdjustment(index) {
        return index === lastShownIndex ? 0 : statusSpacing - spacing
    }

    // The active window's application name, from its desktop entry if any.
    readonly property string activeApplicationName: {
        if (showsDesktopMenus) {
            return panel.shell?.desktopName ?? desktopMenus.name ?? ""
        }
        const window = ToplevelManager.activeToplevel
        if (window === null) {
            return ""
        }
        const entry = DesktopEntries.heuristicLookup(window.appId)
        return entry !== null ? entry.name : window.appId
    }

    // While no window is active, the panel shows the desktop's own menus, from
    // the config's [desktop_menus] (docs/configuration.md). An entry runs a
    // command, asking first if it has a `confirm` table, or one of the shell's
    // actions (shell.qml, run()); one with neither is shown disabled.
    readonly property var desktopMenus: panel.settings.config.desktop_menus ?? null
    readonly property bool showsDesktopMenus:
        desktopMenus !== null && ToplevelManager.activeToplevel === null

    // Entry ids start at a multiple of this per menu, so an id tells its menu.
    readonly property int entriesPerMenu: 1000

    readonly property var desktopMenuItems: {
        if (desktopMenus === null) {
            return []
        }
        return (desktopMenus.menus ?? []).map((menu, menuNumber) => ({
            id: (menuNumber + 1) * entriesPerMenu,
            label: menu.title ?? "",
            enabled: true, separator: false, checkable: false, checked: false, shortcut: [],
            children: (menu.entries ?? []).map((entry, entryNumber) => ({
                id: (menuNumber + 1) * entriesPerMenu + entryNumber + 1,
                label: entry.label ?? "",
                enabled: entry.command !== undefined || entry.action !== undefined,
                separator: entry.separator ?? false,
                checkable: false, checked: false, shortcut: [], children: []
            }))
        }))
    }

    function runDesktopEntry(id) {
        const menu = (desktopMenus.menus ?? [])[Math.floor(id / entriesPerMenu) - 1]
        const entry = (menu?.entries ?? [])[id % entriesPerMenu - 1]
        if (entry?.action !== undefined) {
            panel.shell?.run(entry.action)
        } else if (entry?.command !== undefined) {
            // Commands do not pass through a shell, so "~" is expanded here.
            const home = Quickshell.env("HOME")
            const command = entry.command.map(word => word.startsWith("~") ? home + word.substring(1) : word)
            panel.runEntry({ command: command, confirm: entry.confirm })
        }
    }

    function chooseMenuEntry(id) {
        if (showsDesktopMenus) {
            runDesktopEntry(id)
        } else {
            panel.applicationMenu.activate(id)
        }
    }
    function menuAboutToShow(id) {
        if (!showsDesktopMenus) {
            panel.applicationMenu.aboutToShow(id)
        }
    }

    // If the first menu is titled like the application, it is the app's own
    // menu: its entries go under the app name title, and the row starts after it.
    // The desktop's first menu is always its own, whatever it is called, so
    // that renaming the desktop (Settings, Desktop) keeps it under the name.
    readonly property var applicationMenus:
        showsDesktopMenus ? desktopMenuItems : (panel.applicationMenu?.items ?? [])
    readonly property var ownMenu: {
        if (applicationMenus.length === 0 || activeApplicationName === "") {
            return null
        }
        if (showsDesktopMenus || applicationMenus[0].label === activeApplicationName) {
            return applicationMenus[0]
        }
        return null
    }
    readonly property int firstOrdinaryMenu: ownMenu !== null ? 1 : 0

    SystemClock {
        id: clock
        precision: SystemClock.Minutes
    }

    Repeater {
        id: items
        model: row.names

        Loader {
            required property string modelData
            required property int index
            property int itemIndex: index
            anchors.verticalCenter: parent.verticalCenter
            visible: item === null || (item.shown ?? true)

            sourceComponent: {
                switch (modelData) {
                case "logo": return logoItem
                case "app_name": return appNameItem
                case "menus": return menusItem
                case "clock": return clockItem
                default: return statusItemHolder
                }
            }

            property string itemName: modelData
        }
    }

    component PanelText: Text {
        color: ThemeValues.color(row.panel.looks.text_color)
        font.family: row.panel.font.family
        font.pixelSize: row.panel.font.size
        verticalAlignment: Text.AlignVCenter
    }

    Component {
        id: logoItem
        Item {
            visible: row.panel.looks.logo !== undefined

            // panel.logo_spacing: another gap after the logo, since logo pictures often
            // carry margins; the item is resized by the difference.
            readonly property real gapAdjustment:
                (row.panel.looks.logo_spacing ?? row.spacing) - row.spacing
            width: logoTitle.width + gapAdjustment
            height: row.height

            // Entries from panel.logo_menu; without any, the logo is only a picture.
            readonly property var entries: row.panel.layout.logo_menu ?? []

            PanelTitle {
                id: logoTitle
                panel: row.panel
                itemRow: row
                enabled: parent.entries.length > 0
                picture: row.panel.looks.logo ?? ""
                openPicture: row.panel.looks.logo_highlighted ?? ""
                boxOverhang: row.panel.looks.logo_highlight_overhang ?? row.spacing / 2

                // An entry is { label, command, confirm? }, { label, action } or
                // { separator = true }; its
                // index is its id.
                menuItems: parent.entries.map((entry, index) => ({
                    id: index,
                    label: entry.label ?? "",
                    enabled: true,
                    separator: entry.separator ?? false,
                    checkable: false, checked: false, shortcut: [], children: []
                }))
                onChosen: id => row.panel.runEntry(parent.entries[id])
            }
        }
    }

    Component {
        id: appNameItem
        PanelTitle {
            panel: row.panel
            itemRow: row
            visible: row.activeApplicationName !== ""
            text: row.activeApplicationName
            bold: row.panel.looks.app_name_bold ?? false

            // Every app gets "Quit" (worded by the config, %1 the app's name), after
            // the entries of its own menu if it has one. Apps number entries from 1,
            // so 0 is free for "Quit" and -1 for the separator.
            readonly property int quitId: 0
            readonly property var quitEntry: ({
                id: quitId,
                label: (row.panel.layout.quit_label ?? "Quit %1").arg(row.activeApplicationName),
                enabled: true, separator: false, checkable: false, checked: false,
                shortcut: [], children: []
            })
            readonly property var lineEntry: ({
                id: -1, label: "", enabled: true, separator: true, checkable: false, checked: false,
                shortcut: [], children: []
            })
            // The desktop has no window to close, so no "Quit".
            menuItems: {
                const own = row.ownMenu !== null ? row.ownMenu.children : []
                if (row.showsDesktopMenus) {
                    return own
                }
                return own.length > 0 ? own.concat([lineEntry, quitEntry]) : [quitEntry]
            }
            onOpening: {
                if (row.ownMenu !== null) {
                    row.menuAboutToShow(row.ownMenu.id)
                }
            }
            onChosen: id => {
                if (id === quitId) {
                    ToplevelManager.activeToplevel?.close()
                } else {
                    row.chooseMenuEntry(id)
                }
            }
        }
    }

    Component {
        id: menusItem
        Item {
            width: titles.width
            height: row.height

            Row {
                id: titles
                spacing: row.spacing
                height: parent.height

                // Titles are made per position, not per menu object: applications re-send
                // their whole menu tree often, and rebuilt titles would forget being open.
                Repeater {
                    model: Math.max(0, row.applicationMenus.length - row.firstOrdinaryMenu)

                    PanelTitle {
                        required property int index
                        // Briefly undefined while a shorter menu bar replaces a longer one.
                        readonly property var topMenu: row.applicationMenus[index + row.firstOrdinaryMenu]

                        panel: row.panel
                        itemRow: row
                        text: topMenu?.label ?? ""
                        menuItems: topMenu?.children ?? []
                        onOpening: row.menuAboutToShow(topMenu.id)
                        onChosen: id => row.chooseMenuEntry(id)
                    }
                }
            }
        }
    }

    Component {
        id: clockItem
        Item {
            width: clockTitle.width + row.statusGapAdjustment(parent.itemIndex)
            height: row.height

            PanelTitle {
                id: clockTitle
                panel: row.panel
                itemRow: row
                boxOverhang: row.statusSpacing / 2
                text: Qt.formatDateTime(clock.date, row.panel.layout.clock_format ?? "hh:mm")

                // A date line, if panel.clock_menu_date_format is set.
                readonly property string dateFormat: row.panel.layout.clock_menu_date_format ?? ""
                menuItems: dateFormat === "" ? [] : [{
                    id: 0,
                    label: clock.date.toLocaleDateString(Qt.locale(), dateFormat),
                    enabled: false, separator: false, checkable: false, checked: false,
                    shortcut: [], children: []
                }]
            }
        }
    }

    Component {
        id: statusItemHolder
        Item {
            id: holder

            readonly property string name: parent.itemName
            readonly property bool loaded: fileLoader.status === Loader.Ready
            readonly property bool shown: !loaded || (fileLoader.item.shown ?? true)

            // StatusItem's `gapBefore`: the item is shifted and the holder resized.
            readonly property real leadAdjustment:
                loaded && fileLoader.item.gapBefore !== undefined
                    ? fileLoader.item.gapBefore - row.statusSpacing : 0
            width: (loaded ? fileLoader.item.width : notFound.implicitWidth)
                + row.statusGapAdjustment(parent.itemIndex) + leadAdjustment
            height: row.height

            readonly property var folders: {
                const list = []
                for (const folder of (row.panel.layout.item_folders ?? [])) {
                    list.push("file://" + folder)
                }
                const ofExtension = row.panel.extensions?.folderOfItem(name) ?? ""
                if (ofExtension !== "") {
                    list.push("file://" + ofExtension)
                }
                list.push(Qt.resolvedUrl("status"))
                return list
            }
            property int folderNumber: 0

            function tryFolder() {
                fileLoader.setSource(folders[folderNumber] + "/" + name + ".qml",
                                     { panel: row.panel, itemRow: row })
            }

            // An item may be defined in the config alone (StatusFromConfig.qml); a file
            // of that name wins.
            readonly property var itemConfig: ((row.panel.settings.config.status ?? ({}))[name]) ?? ({})
            readonly property bool madeInConfig:
                itemConfig.command !== undefined || itemConfig.menu !== undefined
            property bool usesConfig: false
            function useConfig() {
                usesConfig = true
                fileLoader.setSource(Qt.resolvedUrl("StatusFromConfig.qml"),
                                     { panel: row.panel, itemRow: row, name: name })
            }

            Component.onCompleted: tryFolder()

            Loader {
                id: fileLoader
                x: holder.leadAdjustment
                height: parent.height
                onStatusChanged: {
                    if (status !== Loader.Error || holder.usesConfig) {
                        return
                    }
                    // Not in this folder: try the next, then the config.
                    if (holder.folderNumber + 1 < holder.folders.length) {
                        holder.folderNumber += 1
                        holder.tryFolder()
                    } else if (holder.madeInConfig) {
                        holder.useConfig()
                    }
                }
            }

            PanelText {
                id: notFound
                visible: fileLoader.status === Loader.Error
                anchors.verticalCenter: parent.verticalCenter
                text: "[" + holder.name + "?]"
            }
        }
    }
}
