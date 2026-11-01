// The local files dropped from another program, for the dock, the desktop and
// the Theme Installer's well.
//
// text/uri-list is the usual place for them, but when the program offers a
// portal transfer (Dolphin always does), Qt fills it in through the document
// portal. Where that fails, Qt hands back the transfer's key instead of the
// files, in text/uri-list and text/plain alike. KDE programs also list the
// files in application/x-kde4-urilist, which Qt passes on as it is; programs
// without a uri-list put the paths in the plain text.

.pragma library

const formats = ["text/uri-list", "application/x-kde4-urilist", "text/plain"]

// The dropped files as paths, from the first format that names any; [] when
// none does.
function paths(drop) {
    for (const format of formats) {
        const found = []
        for (const line of drop.getDataAsString(format).split(/\r?\n/)) {
            if (line.startsWith("file://")) {
                found.push(decodeURIComponent(line.substring("file://".length)))
            } else if (line.startsWith("/")) {
                found.push(line)
            }
        }
        if (found.length > 0) {
            return found
        }
    }
    return []
}
