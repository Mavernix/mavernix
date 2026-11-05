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
    // Many bindings ask this for every window: the search through every
    // entry is kept, until the entries change in number.
    const all = entries.applications.values
    const kept = steamGames[appId]
    if (kept !== undefined && kept.count === all.length) {
        return kept.entry
    }
    const launch = "steam://rungameid/" + game[1]
    const launcher = all.find(entry =>
        (entry.execString ?? (entry.command ?? []).join(" ")).split(/\s+/).includes(launch)) ?? null
    steamGames[appId] = { count: all.length, entry: launcher }
    return launcher
}

// "steam_app_N" -> { count: how many entries there were, entry }.
const steamGames = {}
