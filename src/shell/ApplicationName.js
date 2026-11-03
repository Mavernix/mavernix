// An application's name wherever the desktop shows it: the one the user gave
// it (the config's [app_names], by desktop entry ID, set in Settings, App
// Icons & Names), else its desktop entry's.

.pragma library

// Undefined without an entry, so callers can fall back to an app ID.
// `choices` is the shell's UserChoices, or null.
function of(entry, choices) {
    if (!entry) {
        return undefined
    }
    // String(): a name typed into the config by hand may be a number.
    const chosen = choices ? String(choices.value("app_names", entry.id) ?? "").trim() : ""
    return chosen !== "" ? chosen : entry.name
}
