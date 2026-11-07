// Starts a program's desktop entry, or one of its actions ("New Window"),
// through mavernix-start-program: as its own systemd service, so the
// journal tells how it ended (see src/session/bin/mavernix-start-program).
// Quickshell's own execute() would run it as a child of the shell.

.pragma library

// `quickshell` is the Quickshell global, which a library can't name itself.
// `action` is null for the program itself.
function start(quickshell, entry, action) {
    if (!entry) {
        return
    }
    const command = (action ?? entry).command ?? []
    if (command.length === 0) {
        return
    }
    // execDetached takes no object from a library, so the entry's folder
    // (its Path key) goes along as --in.
    const folder = entry.workingDirectory ?? ""
    quickshell.execDetached(["mavernix-start-program"]
        .concat(folder !== "" ? ["--in", folder] : [], [entry.id], command))
}
