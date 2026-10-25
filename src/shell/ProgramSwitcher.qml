// The program switcher: while a modifier is held, a plate shows the icons
// of programs with windows, most recently used first; each press marks
// the next, and releasing the modifier goes to the marked program.
//
// `step(1)` and `step(-1)` open it and move the mark (the actions
// "switch_program" and "switch_program_back"). Theme: [program_switcher];
// a theme without it has no switcher.

import QtQuick
import Quickshell
import Quickshell.Wayland
import Shell.Native
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: switcher

    IconSource {
        id: themeIcons
        choices: switcher.choices
    }

    // UserChoices, for chosen icons and a rebound switcher key; may be null.
    property var choices: null

    required property var settings

    readonly property var looks: settings.theme.program_switcher ?? null
    readonly property var fonts: settings.theme.fonts
    readonly property string themeDirectory: settings.theme_directory

    // --- Which programs, in which order ----------------------------------------

    // Windows by last use, latest first. The foreign-toplevel protocol does not
    // report this, so it is tracked here.
    property var recentWindows: []

    function noteActive(window) {
        if (window === null) {
            return
        }
        recentWindows = [window].concat(recentWindows.filter(other => other !== window))
    }
    Connections {
        target: ToplevelManager
        function onActiveToplevelChanged() {
            switcher.noteActive(ToplevelManager.activeToplevel)
        }
    }

    // [{ appId, windows }], most recently used first; windows never active come
    // last, in the compositor's order.
    function programsNow() {
        const open = ToplevelManager.toplevels.values
        const ordered = recentWindows.filter(window => open.includes(window))
            .concat(open.filter(window => !recentWindows.includes(window)))
        const programs = []
        for (const window of ordered) {
            let program = programs.find(known => known.appId === window.appId)
            if (program === undefined) {
                program = { appId: window.appId, windows: [] }
                programs.push(program)
            }
            program.windows.push(window)
        }
        return programs
    }

    // --- Opening, stepping, going ----------------------------------------------

    // The programs shown (fixed while the switcher is open) and the marked one.
    property var programs: []
    property int marked: 0
    property bool open: false

    // The modifier of the switching shortcut ("super" for "<super> KEY_TAB").
    readonly property string heldModifier: {
        const key = (choices !== null
            ? choices.value("shortcuts", "switch_program")
            : (settings.config.shortcuts ?? ({})).switch_program) ?? ""
        for (const modifier of ["super", "alt", "ctrl"]) {
            if (key.includes("<" + modifier + ">")) {
                return modifier
            }
        }
        return ""
    }

    function step(direction) {
        if (looks === null) {
            return
        }
        if (!open) {
            programs = programsNow()
            if (programs.length === 0) {
                return
            }
            // The first press marks the program used before this one.
            marked = 0
            open = true
            plate.opacity = 0
            appearing.restart()
            watchingKey.restart()
        }
        marked = (marked + direction + programs.length) % programs.length
    }

    // Activates the marked program's most recently used window.
    function go() {
        if (!open) {
            return
        }
        open = false
        const program = programs[marked]
        if (program !== undefined && program.windows.length > 0) {
            const window = program.windows[0]
            if (window.minimized) {
                // The receiver restores it, since it may be on another desktop.
                minimizedChosen(window)
            } else {
                window.activate()
            }
        }
    }

    signal minimizedChosen(var window)

    function cancel() {
        open = false
    }

    HeldKeys {
        id: heldKeys
    }

    // Falls back to the generic program icon.
    function iconFor(entry) {
        return themeIcons.forEntry(entry, "application-x-executable")
    }
    // The modifier is polled instead of waiting for its release: after a quick
    // tap the release goes to another window before this one has the keyboard,
    // and the switcher would stay open. The first poll waits until the
    // compositor has given us the keyboard and the held keys.
    Timer {
        id: watchingKey
        interval: 120
        repeat: true
        running: false
        onTriggered: {
            if (!switcher.open) {
                stop()
                return
            }
            interval = 30
            if (switcher.heldModifier === "" || !heldKeys.held(switcher.heldModifier)) {
                stop()
                switcher.go()
            }
        }
        onRunningChanged: {
            if (!running) {
                interval = 120
            }
        }
    }
    // Only shown once the key is held this long, so a quick tap never flashes it.
    Timer {
        id: appearing
        interval: switcher.looks?.appear_after_ms ?? 150
        onTriggered: plate.opacity = 1
    }

    // --- The window -------------------------------------------------------------

    visible: open
    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    // Needed so the compositor reports held keys, and for Escape and arrows.
    WlrLayershell.keyboardFocus: open ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    WlrLayershell.namespace: "desktop-program-switcher"
    color: "transparent"
    // Presses go through to what is under it, except on the plate.
    mask: Region {
        item: plate
    }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: switcher.cancel()
        Keys.onLeftPressed: switcher.step(-1)
        Keys.onRightPressed: switcher.step(1)
    }

    readonly property real iconSize: looks?.icon_size ?? 64
    readonly property real iconGap: looks?.icon_gap ?? 8
    readonly property var padding: looks?.padding ?? [16, 16, 16, 16]   // top, right, bottom, left

    Item {
        id: plate
        anchors.centerIn: parent
        width: switcher.padding[3] + switcher.programs.length * switcher.iconSize
            + Math.max(0, switcher.programs.length - 1) * switcher.iconGap + switcher.padding[1]
        height: switcher.padding[0] + switcher.iconSize + switcher.padding[2]

        Fill {
            anchors.fill: parent
            fill: switcher.looks?.background ?? ({ color: "#000000a0", radius: 12 })
            themeDirectory: switcher.themeDirectory
        }

        Repeater {
            model: switcher.programs

            Item {
                id: programItem
                required property var modelData
                required property int index
                readonly property var entry: DesktopEntries.heuristicLookup(modelData.appId)
                readonly property bool isMarked: index === switcher.marked

                x: switcher.padding[3] + index * (switcher.iconSize + switcher.iconGap)
                y: switcher.padding[0]
                width: switcher.iconSize
                height: switcher.iconSize

                Fill {
                    readonly property real reach: switcher.looks?.mark_reach ?? 4
                    visible: programItem.isMarked
                    anchors.fill: parent
                    anchors.margins: -reach
                    fill: switcher.looks?.mark ?? ({ color: "#ffffff30", radius: 8 })
                    themeDirectory: switcher.themeDirectory
                }
                Image {
                    anchors.fill: parent
                    sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                    source: switcher.iconFor(programItem.entry)
                }
                // The marked program's name, on a plate of its own if the theme gives one.
                Item {
                    visible: programItem.isMarked
                    readonly property var name: switcher.looks?.name ?? ({})
                    width: label.implicitWidth + 2 * (name.padding ?? 8)
                    height: name.height ?? (label.implicitHeight + 4)
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height + (name.gap ?? 6)

                    Fill {
                        anchors.fill: parent
                        fill: parent.name.background ?? ({ color: "#00000000" })
                        themeDirectory: switcher.themeDirectory
                    }
                    Text {
                        id: label
                        anchors.centerIn: parent
                        text: programItem.entry?.name ?? programItem.modelData.appId
                        color: ThemeValues.color(parent.name.color ?? "#ffffff")
                        readonly property var face: switcher.fonts.program_switcher ?? switcher.fonts.interface
                        font.family: face.family
                        font.pixelSize: face.size
                        font.bold: face.bold ?? false
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: switcher.marked = programItem.index
                    onClicked: {
                        switcher.marked = programItem.index
                        switcher.go()
                    }
                }
            }
        }
    }
}
