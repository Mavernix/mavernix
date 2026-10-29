// What the dock's menus list: a place's menu on a secondary press (a
// program, the trash, a minimised window, a folder, the dock itself) and a
// folder shown as a list. Wording is the config's (dock.words). Each item
// has an id; the dock (Dock.qml, menuChosen) does what the chosen id says.
//
// Items are plain objects in MenuBox's form.

import QtQuick

QtObject {
    id: menus

    // The config's [dock] table and its words, and the dock's DockFolders (dockFolders).
    required property var config
    required property var words
    required property var dockFolders
    // What some entries say: whether magnification is on, whether the trash
    // is empty.
    property bool magnificationWanted: false
    property bool trashIsEmpty: true

    // --- Ids ------------------------------------------------------------------------

    readonly property int keepId: 1
    readonly property int removeId: 2
    readonly property int openId: 4
    readonly property int quitId: 5
    readonly property int emptyTrashId: 6
    readonly property int magnificationId: 7
    readonly property int openFolderId: 8
    readonly property int settingsId: 9
    // Ranges of ids, kept in ascending order: the dock tests them from the
    // highest down. A program's windows, a folder's orders and views, a
    // program's own entries (its desktop entry's actions), a listed folder's
    // files.
    readonly property int windowBaseId: 100
    readonly property int orderBaseId: 200
    readonly property int viewBaseId: 300
    readonly property int actionBaseId: 400
    readonly property int fileBaseId: 10000

    function entry(id, label, enabled, children) {
        return { id: id, label: label, enabled: enabled ?? true, separator: false,
                 checkable: false, checked: false, shortcut: [], children: children ?? [] }
    }

    function separator() {
        return { id: -1, label: "", enabled: false, separator: true,
                 checkable: false, checked: false, shortcut: [], children: [] }
    }

    function ticked(id, label, isChosen) {
        const item = entry(id, label)
        item.checkable = true
        item.checked = isChosen
        return item
    }

    // --- A place's menu -----------------------------------------------------------

    // The menu of a place in the dock (Dock.qml's places), whose tile is `tile`.
    function forPlace(place, tile) {
        if (place.isSeparator) {
            return forDock()
        }
        if (place.isTrash) {
            return [
                entry(openId, words.open ?? "Open"),
                separator(),
                // Nothing to empty, or no command for it: greyed.
                entry(emptyTrashId, words.empty_trash ?? "Empty trash",
                      !trashIsEmpty && config.trash_empty !== undefined)
            ]
        }
        if (place.window !== null) {
            return [entry(openId, (words.open_named ?? "Open “%1”").arg(place.window.title))]
        }
        if (place.custom?.folder !== undefined || dockFolders.isProgramFolder(place.custom)) {
            return forFolder(place.custom)
        }
        return forProgram(place, tile)
    }

    // The dock's own menu, from a separator.
    function forDock() {
        const items = [entry(magnificationId, magnificationWanted
            ? (words.magnification_off ?? "Turn magnification off")
            : (words.magnification_on ?? "Turn magnification on"))]
        if (config.settings_command !== undefined) {
            items.push(separator())
            items.push(entry(settingsId, words.settings ?? "Dock settings…"))
        }
        return items
    }

    function forProgram(place, tile) {
        const items = []
        const windows = tile.windows
        for (let number = 0; number < windows.length; number++) {
            items.push(ticked(windowBaseId + number, windows[number].title, windows[number].activated))
        }
        if (windows.length > 0) {
            items.push(separator())
        }

        // The program's own entries, such as "New Window", from its desktop
        // entry, after its windows as on the original. The original offers
        // them only while the program runs; [dock] actions_when_closed
        // offers them always, to start a program straight into one.
        const actions = tile.entry?.actions ?? []
        if (actions.length > 0 && (tile.running || (config.actions_when_closed ?? true))) {
            for (let number = 0; number < actions.length; number++) {
                items.push(entry(actionBaseId + number, actions[number].name))
            }
            items.push(separator())
        }

        // A running program can be kept or let go; a kept idle one only removed.
        // One without a desktop entry (no listedAs) cannot be kept. In the
        // menu itself rather than the original's Options submenu, to be found.
        if (place.listedAs !== undefined) {
            if (tile.running) {
                items.push(ticked(keepId, words.keep ?? "Keep in dock", place.kept))
            } else {
                items.push(entry(removeId, words.remove ?? "Remove from dock"))
            }
            items.push(separator())
        }
        items.push(tile.running ? entry(quitId, words.quit ?? "Quit") : entry(openId, words.open ?? "Open"))
        return items
    }

    // A folder's menu: its order and view, removing it, opening it.
    function forFolder(folder) {
        const heading = label => entry(-1, label, false)
        const orderWords = [words.sort_name ?? "Name", words.sort_added ?? "Date added",
                            words.sort_modified ?? "Date modified",
                            words.sort_created ?? "Date created", words.sort_kind ?? "Kind"]
        const viewWords = { fan: words.view_fan ?? "Fan", grid: words.view_grid ?? "Grid",
                            list: words.view_list ?? "List" }

        const items = [heading(words.sort_by ?? "Sort by")]
        dockFolders.orders.forEach((order, number) => items.push(
            ticked(orderBaseId + number, orderWords[number], (folder.order ?? "name") === order)))
        items.push(separator())
        items.push(heading(words.view_as ?? "Show contents as"))
        dockFolders.views.forEach((view, number) => items.push(
            ticked(viewBaseId + number, viewWords[view], dockFolders.viewOf(folder) === view)))
        items.push(separator())
        items.push(entry(removeId, words.remove ?? "Remove from dock"))
        items.push(separator())
        items.push(entry(openFolderId, (words.open_named ?? "Open “%1”").arg(folder.name ?? "")))
        return items
    }

    // --- A folder as a list --------------------------------------------------------

    // The list of a folder's entries, then the entry that opens the folder:
    // { items, files }, where the files are the ones the items offer, a
    // file's id being fileBaseId plus its place among them. `icons` is an
    // IconSource.
    function folderList(folder, entries, icons) {
        const files = []
        const items = listItems(entries, files, icons)
        if (items.length > 0) {
            items.push(separator())
        }
        items.push(entry(openFolderId, dockFolders.openWholeLabel(folder)))
        return { items: items, files: files }
    }

    // Items for entries, with their icons; a folder inside is a submenu of its
    // own entries. Each file is added to `files`.
    function listItems(entries, files, icons) {
        return entries.map(file => {
            let item
            if (file.entries !== undefined) {
                item = entry(-1, file.name, true, listItems(file.entries, files, icons))
            } else {
                item = entry(fileBaseId + files.length, file.name)
                files.push(file)
            }
            item.icon = icons.forFolderEntry(file)
            return item
        })
    }
}
