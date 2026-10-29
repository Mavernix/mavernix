// What a folder in the dock holds, and how its entries open: a folder of
// files, or the folder of all installed programs. The dock (Dock.qml) keeps
// which folder is open, its menus and the box it shows in.
//
// A custom dock place with `folder` lists that folder's files, by its
// `order`. One with `programs = true` lists the installed programs: there is
// no real folder to read, since a system's programs are desktop entries
// spread over several directories, some of them hidden. Its entries' paths
// are desktop entry IDs, and its last entry opens the application grid
// instead of a file manager. Its `subfolders`, each { name, icon,
// categories }, gather the programs of those desktop entry categories into
// a folder inside it (as the original's Utilities).
//
// An entry is { name, path, icon, genericIcon }, as FolderContents gives
// them; a folder inside also has `entries`, its own.

import QtQuick
import Quickshell
import Shell.Native

QtObject {
    id: folders

    // The config's [dock] table, its words, the shell's root (for the app
    // grid) and the dock's IconSource.
    required property var config
    required property var words
    required property var icons
    property var shell: null

    // The folder whose entries `listedEntries` holds, or null.
    property var listedFolder: null

    readonly property var orders: ["name", "added", "modified", "created", "kind"]
    readonly property var views: ["fan", "grid", "list"]

    function isProgramFolder(folder) {
        return folder?.programs === true
    }

    // How the folder shows its entries: its `view`, if it is one of `views`.
    function viewOf(folder) {
        const fallback = isProgramFolder(folder) ? "grid" : "list"
        const view = folder?.view ?? fallback
        return views.includes(view) ? view : fallback
    }

    // What the listed folder holds.
    readonly property var listedEntries: isProgramFolder(listedFolder)
        ? programEntries(listedFolder) : fileContents.entries

    // --- A folder of files ------------------------------------------------------

    property FolderContents fileContents: FolderContents {
        path: (folders.listedFolder?.folder ?? "") !== "" ? folders.expandedPath(folders.listedFolder.folder) : ""
        order: folders.listedFolder?.order ?? "name"
    }

    property FileKind fileKinds: FileKind {}

    function expandedPath(path) {
        return path.startsWith("~") ? Quickshell.env("HOME") + path.substring(1) : path
    }

    function openFile(path) {
        Quickshell.execDetached((config.open_command ?? ["xdg-open"]).concat([path]))
    }

    // --- The folder of all programs -------------------------------------------

    // The directories desktop entries are installed in, the user's first:
    // "applications" under XDG_DATA_HOME and each of XDG_DATA_DIRS.
    readonly property var programDirectories: {
        const dataHome = Quickshell.env("XDG_DATA_HOME") || Quickshell.env("HOME") + "/.local/share"
        const dataDirs = (Quickshell.env("XDG_DATA_DIRS") || "/usr/local/share:/usr/share").split(":")
        return [dataHome].concat(dataDirs).map(directory => directory.replace(/\/$/, "") + "/applications")
    }

    function isProgramDirectory(path) {
        return programDirectories.includes(path)
    }

    // When a program's desktop entry was added, modified or created, from
    // the file the desktop's own lookup uses: the first directory that has
    // it. 0 if none has it.
    function programMoment(id, order) {
        for (const directory of programDirectories) {
            const moment = fileKinds.momentOf(directory + "/" + id + ".desktop", order)
            if (moment > 0) {
                return moment
            }
        }
        return 0
    }

    // The installed programs as entries, in the folder's order (one of
    // `orders`): by name; by the date of their desktop entry ("added" is
    // when it was installed), newest first; or by "kind", their main
    // category, then by name. Subfolders that have programs come among the
    // programs by name, after them in the other orders.
    function programEntries(folder) {
        const order = folder.order ?? "name"
        const shown = DesktopEntries.applications.values.filter(entry => !entry.noDisplay)
        const byName = (first, second) => first.name.localeCompare(second.name)
        if (order === "kind") {
            const kind = entry => entry.categories.length > 0 ? entry.categories[0] : ""
            shown.sort((first, second) => kind(first).localeCompare(kind(second)) || byName(first, second))
        } else if (order === "added" || order === "modified" || order === "created") {
            const moments = new Map(shown.map(entry => [entry, programMoment(entry.id, order)]))
            shown.sort((first, second) => moments.get(second) - moments.get(first) || byName(first, second))
        } else {
            shown.sort(byName)
        }

        const wanted = folder.subfolders ?? []
        const subfolders = wanted.map(subfolder => ({ name: subfolder.name, path: "",
                                                      icon: subfolder.icon ?? "folder",
                                                      genericIcon: "folder", entries: [] }))
        const top = []
        for (const entry of shown) {
            const asEntry = { name: entry.name, path: entry.id, icon: icons.nameFor(entry),
                              genericIcon: "application-x-executable" }
            const inside = wanted.findIndex(
                subfolder => entry.categories.some(category => (subfolder.categories ?? []).includes(category)))
            if (inside >= 0) {
                subfolders[inside].entries.push(asEntry)
            } else {
                top.push(asEntry)
            }
        }
        const filled = subfolders.filter(subfolder => subfolder.entries.length > 0)
        if (order !== "name") {
            return top.concat(filled)
        }
        return top.concat(filled).sort(byName)
    }

    // --- Opening ------------------------------------------------------------------

    // Opens an entry of the folder: a file with the config's open command,
    // a program by starting it.
    function openEntry(folder, path) {
        if (!isProgramFolder(folder)) {
            openFile(path)
            return
        }
        const entry = DesktopEntries.applications.values.find(candidate => candidate.id === path)
        entry?.execute()
    }

    // Opens the folder itself: in the file manager, or the application grid.
    function openWhole(folder) {
        if (isProgramFolder(folder)) {
            shell?.run("app_grid")
        } else {
            openFile(expandedPath(folder.folder))
        }
    }

    function openWholeLabel(folder) {
        return isProgramFolder(folder) ? (words.open_app_grid ?? "Open app grid")
                                       : (words.open_in_file_manager ?? "Open in file manager")
    }
}
