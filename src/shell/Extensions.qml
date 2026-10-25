// Runs the switched-on extensions' `background` files and tells the panel
// which items they bring. The session finds the extensions
// (`settings.extensions`); [extensions] enabled says which are on. See
// docs/extensions.md.

import QtQuick
import Quickshell

Scope {
    id: extensions

    // `shell` is the shell's root, handed to every extension so that it
    // can call `shell.run(action)`.
    required property var settings
    required property var userChoices
    property var shell: null
    property var screen: null

    // What the session found, with each extension's settings as the user
    // has changed them since (the table of its name under [extensions]).
    readonly property var found: {
        const result = {}
        const atStart = settings.extensions ?? ({})
        for (const name in atStart) {
            const chosen = userChoices.value("extensions", name)
            const changed = (chosen !== undefined && typeof chosen === "object") ? chosen : ({})
            result[name] = Object.assign({}, atStart[name], {
                settings: Object.assign({}, atStart[name].settings, changed)
            })
        }
        return result
    }
    // In the config's order; names no longer installed are left out.
    readonly property var enabledNames:
        ((userChoices.value("extensions", "enabled") ?? [])).filter(name => found[name] !== undefined)

    // { directory, manifest, looks, settings }, or undefined.
    function info(name) {
        return found[name]
    }

    // The object made from an extension's `background` file, shared by its
    // other parts; null if it has none or is switched off.
    function running(name) {
        runningRevision
        for (let index = 0; index < backgrounds.count; index++) {
            const loader = backgrounds.objectAt(index)
            if (loader !== null && loader.name === name) {
                return loader.item
            }
        }
        return null
    }
    // Counts up when a background object comes or goes, so that bindings
    // on `running` are evaluated again.
    property int runningRevision: 0

    // --- For the panel ----------------------------------------------------------------

    // The folder holding a panel item an extension brings, else "".
    function folderOfItem(item) {
        for (const name of enabledNames) {
            const wanted = found[name].manifest.panel_items ?? ({})
            if ((wanted.left ?? []).includes(item) || (wanted.right ?? []).includes(item)) {
                return found[name].directory + "/status"
            }
        }
        return ""
    }

    // `panel_items` from the extensions' extension.toml, for "left" or "right".
    function panelItems(end) {
        const names = []
        for (const name of enabledNames) {
            const wanted = found[name].manifest.panel_items ?? ({})
            for (const item of (wanted[end] ?? [])) {
                names.push(item)
            }
        }
        return names
    }

    // --- What extensions keep running ---------------------------------------------------

    Instantiator {
        id: backgrounds
        model: extensions.enabledNames.filter(name => extensions.found[name].manifest.background !== undefined)

        // A Loader, so that a broken file costs only its own extension.
        delegate: Loader {
            required property string modelData
            readonly property string name: modelData
            readonly property var description: extensions.found[modelData]

            Component.onCompleted: {
                setSource("file://" + description.directory + "/" + description.manifest.background, {
                    // Bound, so that changed settings arrive.
                    extension: Qt.binding(() => description),
                    extensions: extensions,
                    settings: extensions.settings,
                    shell: extensions.shell,
                    screen: Qt.binding(() => extensions.screen)
                })
            }
            onStatusChanged: {
                if (status === Loader.Error) {
                    console.warn("extensions:", name, "could not be started: its file",
                                 description.directory + "/" + description.manifest.background,
                                 "has an error (messages above). The desktop goes on without it.")
                }
                extensions.runningRevision += 1
            }
        }
        onObjectAdded: extensions.runningRevision += 1
        onObjectRemoved: extensions.runningRevision += 1
    }
}
