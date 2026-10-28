// Icons by name, for every part of the shell. Lookups go through the native
// module, which renders SVG icons properly and notices icons installed while
// the shell runs; anything it lacks is asked of Quickshell. An application's
// icon may be one the user chose for it, if `choices` is set.

import QtQuick
import Quickshell
import Shell.Native

IconFiles {
    id: source

    // The shell's UserChoices; null where a component shows no applications.
    property var choices: null

    // The icon of this name, or "" if no icon theme has one.
    function find(name) {
        if ((name ?? "") === "") {
            return ""
        }
        return source.sourceFor(name, source.revision)
    }

    // The icon of this name; if there is none, the icon named `fallback`;
    // without a fallback, Quickshell's picture for a missing icon.
    function path(name, fallback) {
        const own = find(name)
        if (own !== "") {
            return own
        }
        if (fallback === undefined) {
            return Quickshell.iconPath(name)
        }
        const other = find(fallback)
        return other !== "" ? other : Quickshell.iconPath(name, fallback)
    }

    // The file or icon name for an application: the user's choice from the
    // config's [app_icons] table if it still exists, else its desktop entry's.
    function nameFor(entry) {
        const chosen = (entry && choices) ? (choices.value("app_icons", entry.id) ?? "") : ""
        if (chosen !== "" && find(chosen) !== "") {
            return chosen
        }
        return entry?.icon ?? ""
    }

    // The icon address for a desktop entry (null for a window without one).
    function forEntry(entry, fallback) {
        return path(nameFor(entry), fallback)
    }

    // The icon of this name, shrunk to span at most `fill` of its square
    // unless the icon theme has it itself; else as path() gives it.
    function fitted(name, fill, fallback) {
        if (fill > 0 && find(name) !== "" && !source.isThemeOwn(name, source.revision)) {
            return "image://fitted-icon/" + fill + "/" + source.revision + "/" + name
        }
        return path(name, fallback)
    }
}
