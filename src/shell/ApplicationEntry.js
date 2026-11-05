// A window's desktop entry, from its app ID (an X11 program's WM_CLASS).
// Quickshell's heuristicLookup finds most; games that Steam starts call
// their windows "steam_app_N", while the entry Steam makes for the game
// runs "steam steam://rungameid/N", so those are found by that.

.pragma library

// `entries` is Quickshell's DesktopEntries; null if nothing matches.
function of(entries, appId) {
    if (!appId) {
        return null
    }
    const found = entries.heuristicLookup(appId)
    if (found) {
        return found
    }
    const game = /^steam_app_(\d+)$/.exec(appId)
    if (game === null) {
        return null
    }
    const launch = "steam://rungameid/" + game[1]
    for (const entry of entries.applications.values) {
        const runs = entry.execString ?? (entry.command ?? []).join(" ")
        if (runs.split(/\s+/).includes(launch)) {
            return entry
        }
    }
    return null
}
