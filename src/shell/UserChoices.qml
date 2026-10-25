// What the user changes while using the desktop, kept between sessions.
//
// The config holds the defaults and is not the shell's to write; changes go
// to a user file holding only what differs. src/session/user_choices.py
// overlays that file at session start; this object overlays later changes:
//
//   choices.value("dock", "groups")        what holds now
//   choices.set("dock", "groups", groups)  change it, and remember

import QtQuick
import Quickshell
import Quickshell.Io

Item {
    id: choices

    // Its config already has the user file overlaid.
    required property var settings

    // Changes since the session started: { table: { key: value } }.
    property var changed: ({})

    function value(table, key) {
        const changedTable = changed[table]
        if (changedTable !== undefined && changedTable[key] !== undefined) {
            return changedTable[key]
        }
        return (settings.config[table] ?? ({}))[key]
    }

    function set(table, key, newValue) {
        // A new object, so bindings to `changed` re-evaluate.
        const next = Object.assign({}, changed)
        next[table] = Object.assign({}, next[table] ?? ({}))
        next[table][key] = newValue
        changed = next

        const kept = readFile()
        kept[table] = Object.assign({}, kept[table] ?? ({}))
        kept[table][key] = newValue
        file.setText(JSON.stringify(kept, null, 1) + "\n")
        // FileView's watcher does not report our own writes (at least to a file
        // that did not exist before), so the compositor is told from here.
        tellCompositor(kept)
    }

    // The file as an object; empty if missing or not valid JSON.
    function readFile() {
        try {
            const held = JSON.parse(file.text())
            return (typeof held === "object" && held !== null) ? held : {}
        } catch (error) {
            return {}
        }
    }

    FileView {
        id: file
        path: choices.settings.user_choices_file ?? ""
        blockLoading: true
        printErrors: false

        // The settings window writes the file too; follow it at once.
        watchChanges: true
        onFileChanged: reload()
        onLoaded: {
            const held = choices.readFile()
            choices.changed = held
            choices.tellCompositor(held)
        }
    }

    // These tables belong to the compositor, which reads them from a settings
    // file the session writes; `mavernix-reconfigure` rewrites it when the
    // user's file changes in one of them.

    readonly property var compositorTables:
        ["input", "idle", "commands", "shortcuts", "gestures", "display", "window", "desktops"]
    // These tables as last given to the compositor, as JSON text; unset until
    // the file has been read once.
    property var givenToCompositor: undefined

    // Without a file nothing is loaded, so record the "no changes" state here.
    Component.onCompleted: {
        if (givenToCompositor === undefined) {
            tellCompositor(readFile())
        }
    }

    function tellCompositor(held) {
        const part = {}
        for (const table of compositorTables) {
            if (held[table] !== undefined) {
                part[table] = held[table]
            }
        }
        const text = JSON.stringify(part)
        if (givenToCompositor !== undefined && text !== givenToCompositor) {
            Quickshell.execDetached(["mavernix-reconfigure"])
        }
        givenToCompositor = text
    }
}
