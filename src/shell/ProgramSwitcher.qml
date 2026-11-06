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
import "ApplicationEntry.js" as ApplicationEntry
import "ApplicationName.js" as ApplicationName
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
    // Where the pointer was last seen over the plate since it opened.
    property var restingPointer: null

    // The icons on the plate, one row per entry of `programs`. Kept between
    // openings and only reordered, so opening does not build every icon again.
    ListModel {
        id: shownPrograms
    }
    function show(list) {
        for (let i = 0; i < list.length; i++) {
            let found = -1
            for (let k = i; k < shownPrograms.count; k++) {
                if (shownPrograms.get(k).appId === list[i].appId) {
                    found = k
                    break
                }
            }
            if (found === -1) {
                shownPrograms.insert(i, { appId: list[i].appId })
            } else if (found !== i) {
                shownPrograms.move(found, i, 1)
            }
        }
        if (shownPrograms.count > list.length) {
            shownPrograms.remove(list.length, shownPrograms.count - list.length)
        }
    }

    // Once the switcher has the keyboard, the compositor's shortcuts are
    // paused, so Tab comes to this window instead of as a new shortcut.
    ShortcutPause {
        paused: switcher.open && keyTaker.activeFocus
    }

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
            show(programs)
            restingPointer = null
            // The first press marks the program used before this one.
            marked = 0
            open = true
            plate.opacity = 0
            appearing.restart()
        }
        marked = (marked + direction + programs.length) % programs.length
    }

    // Activates the marked program's most recently used window.
    function go() {
        if (!open) {
            return
        }
        // Asked for before the plate closes: taking its window down takes
        // a while (about 15 ms with software rendering), and the compositor
        // gives the keyboard to the chosen window once it is gone.
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
        open = false
    }

    signal minimizedChosen(var window)

    function cancel() {
        open = false
    }

    // Whether `key` is the held modifier, for its release.
    function isHeldModifier(key) {
        if (heldModifier === "super") {
            return key === Qt.Key_Super_L || key === Qt.Key_Super_R || key === Qt.Key_Meta
        }
        if (heldModifier === "alt") {
            return key === Qt.Key_Alt
        }
        if (heldModifier === "ctrl") {
            return key === Qt.Key_Control
        }
        return false
    }

    // Goes once the modifier is let go. The compositor is asked, as a quick
    // tap is over before this window has the keyboard, and then the release
    // went to another window. While this window has the keyboard, the
    // release also comes as a key (see the keys below), which is sooner.
    HeldKeys {
        modifier: switcher.heldModifier
        watching: switcher.open
        onLetGo: switcher.go()
    }

    // Falls back to the generic program icon.
    function iconFor(entry) {
        return themeIcons.forEntry(entry, "application-x-executable")
    }
    // Only shown once the key is held this long, so a quick tap never flashes it.
    Timer {
        id: appearing
        interval: switcher.looks?.appear_after_ms ?? 150
        onTriggered: plate.opacity = 1
    }

    // --- The window -------------------------------------------------------------

    visible: open && keepShown.canShow
    KeepShown {
        id: keepShown
        target: switcher
    }
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
        id: keyTaker
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: switcher.cancel()
        Keys.onLeftPressed: switcher.step(-1)
        Keys.onRightPressed: switcher.step(1)
        // While the compositor's shortcuts are paused, Tab comes here.
        Keys.onTabPressed: switcher.step(1)
        Keys.onBacktabPressed: switcher.step(-1)
        Keys.onReleased: event => {
            if (switcher.isHeldModifier(event.key)) {
                switcher.go()
            }
        }
    }

    readonly property real iconGap: looks?.icon_gap ?? 8
    readonly property var padding: looks?.padding ?? [16, 16, 16, 16]   // top, right, bottom, left
    // The theme's size, smaller when that many icons would not fit: the plate
    // stays inside the screen by its own padding on each side.
    readonly property real iconSize: {
        const wanted = looks?.icon_size ?? 64
        const count = Math.max(1, shownPrograms.count)
        const screenWidth = screen?.width ?? width
        const room = screenWidth - 2 * (padding[1] + padding[3]) - (count - 1) * iconGap
        return Math.max(1, Math.min(wanted, Math.floor(room / count)))
    }

    Item {
        id: plate
        anchors.centerIn: parent
        width: switcher.padding[3] + shownPrograms.count * switcher.iconSize
            + Math.max(0, shownPrograms.count - 1) * switcher.iconGap + switcher.padding[1]
        height: switcher.padding[0] + switcher.iconSize + switcher.padding[2]

        Fill {
            anchors.fill: parent
            fill: switcher.looks?.background ?? ({ color: "#000000a0", radius: 12 })
            themeDirectory: switcher.themeDirectory
        }

        Repeater {
            model: shownPrograms

            Item {
                id: programItem
                required property string appId
                required property int index
                readonly property var entry: ApplicationEntry.of(DesktopEntries, appId)
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
                    // Loaded once per program, as the icons are kept.
                    asynchronous: true
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
                        text: ApplicationName.of(programItem.entry, switcher.choices) ?? programItem.appId
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
                    // Only a moved pointer marks: one resting where the plate
                    // appears would otherwise take the mark from the keys (Qt
                    // reports a position for it as the plate appears).
                    onPositionChanged: mouse => {
                        const at = mapToItem(null, mouse.x, mouse.y)
                        const resting = switcher.restingPointer
                        switcher.restingPointer = at
                        if (resting !== null && (resting.x !== at.x || resting.y !== at.y)) {
                            switcher.marked = programItem.index
                        }
                    }
                    onClicked: {
                        switcher.marked = programItem.index
                        switcher.go()
                    }
                }
            }
        }
    }
}
