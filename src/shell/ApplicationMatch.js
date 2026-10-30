// How well an application answers what was typed into a search field, for
// Search.qml and the application grid. As on a Mac, an application is found
// by its name, and also by what it is: "term" finds a terminal called Foot
// by its generic name, "Terminal", or by its keywords.

.pragma library

// 2: the name holds the text; 1: a word of the generic name (as in "Web
// Browser") or a keyword begins with it; 0: neither. `wanted` is in lower
// case and not empty.
function strength(entry, wanted) {
    if (entry.name.toLowerCase().includes(wanted)) {
        return 2
    }
    const words = (entry.genericName ?? "").split(/\s+/).concat(Array.from(entry.keywords ?? []))
    return words.some(word => word.toLowerCase().startsWith(wanted)) ? 1 : 0
}
