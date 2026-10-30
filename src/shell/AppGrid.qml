// The application grid: every installed application as an icon with its
// name, in pages that cover the whole screen. Typing narrows the icons; a
// touchpad, a drag, the wheel, the arrow keys or the page dots turn pages.
// Looks are the theme's [app_grid], [animation.app_grid] and
// [animation.app_grid_page] (docs/themes/, "Application grid").

import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import "ApplicationMatch.js" as ApplicationMatch
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: grid

    IconSource {
        id: themeIcons
        choices: grid.choices
    }

    // The shell's UserChoices, for the icon the user chose for an
    // application; null if there is none to ask.
    property var choices: null

    function iconFor(entry) {
        return themeIcons.forEntry(entry, "application-x-executable")
    }

    // The settings object written by src/session/make_shell_config.py.
    required property var settings
    property var desktopFill: settings.theme.desktop.background

    readonly property var theme: settings.theme
    readonly property var looks: theme.app_grid ?? ({})
    readonly property var searchLooks: looks.search ?? ({})
    readonly property var dotLooks: looks.page_dots ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    // { kind: "none" | "fade" | "zoom", duration_ms, close_duration_ms }.
    readonly property var motion: (theme.animation ?? ({})).app_grid ?? ({ kind: "none" })
    // With "zoom", the icons come from this size and go back to it: a little
    // larger than at rest. They settle in decelerating and leave
    // accelerating, while the fade beside them keeps an even pace.
    readonly property real awayScale: 1.1
    readonly property int openDuration: motion.kind === "none" ? 0 : (motion.duration_ms ?? 150)
    readonly property int closeDuration: motion.kind === "none" ? 0
                                         : (motion.close_duration_ms ?? openDuration)

    // { kind: "none" | "slide", duration_ms, curve }.
    readonly property var pageMotion: (theme.animation ?? ({})).app_grid_page ?? ({ kind: "none" })

    property bool open: false

    // The shell's dock, for "Keep in Dock" in a program's menu; may be null.
    property var dock: null

    // An application's icon is being dragged (the dock listens, to take it
    // in). `dragGivenUp` means the grid was closed under the drag.
    signal dragMoved(var entry, string iconSource, real screenX, real screenY)
    signal dragEnded()
    signal dragGivenUp()
    property var draggedEntry: null

    function toggle() {
        if (open) {
            close()
        } else {
            searchField.text = ""
            jumpToPage(0)
            open = true
        }
    }

    function close() {
        if (draggedEntry !== null) {
            draggedEntry = null
            dragGivenUp()
        }
        programMenu.open = false
        open = false
    }

    // The window stays until the going-away motion is over.
    visible: open || content.opacity > 0

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    exclusionMode: ExclusionMode.Ignore
    // Among surfaces of one layer the compositor promises no order, so while
    // the grid is open the dock asks to be above it and the panel hides.
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: open ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    // --- What is shown --------------------------------------------------------

    readonly property var applications: {
        const wanted = searchField.text.trim().toLowerCase()
        const list = []
        for (const entry of DesktopEntries.applications.values) {
            if (entry.noDisplay) {
                continue
            }
            const strength = wanted === "" ? 0 : ApplicationMatch.strength(entry, wanted)
            if (wanted !== "" && strength === 0) {
                continue
            }
            list.push({ entry: entry, strength: strength })
        }
        // Found by name first, then by what they are or their keywords.
        list.sort((first, second) => second.strength - first.strength
                                     || first.entry.name.localeCompare(second.entry.name))
        return list.map(found => found.entry)
    }

    readonly property int columns: looks.columns ?? 6
    readonly property int rows: looks.rows ?? 4
    readonly property int perPage: columns * rows
    readonly property int pageCount: Math.max(1, Math.ceil(applications.length / perPage))
    onPageCountChanged: jumpToPage(Math.min(page, pageCount - 1))

    // --- Which page is shown --------------------------------------------------

    // Where the strip of pages stands, counted in pages: 0.5 shows half of
    // the first and half of the second.
    property real stripPosition: 0
    readonly property int page: Math.max(0, Math.min(pageCount - 1, Math.round(stripPosition)))

    NumberAnimation {
        id: slide
        target: grid
        property: "stripPosition"
        duration: grid.pageMotion.duration_ms ?? 250
        easing.type: ThemeValues.easing(grid.pageMotion.curve)
    }

    function jumpToPage(number) {
        slide.stop()
        stripPosition = number
    }

    function slideToPage(number) {
        const target = Math.max(0, Math.min(pageCount - 1, number))
        if (pageMotion.kind === "none") {
            jumpToPage(target)
            return
        }
        slide.stop()
        slide.from = stripPosition
        slide.to = target
        slide.start()
    }

    // Counted from where a slide under way is going, so that two quick turns
    // go two pages.
    function turnPage(by) {
        slideToPage((slide.running ? slide.to : page) + by)
    }

    // --- Pulling the strip by hand ---------------------------------------------
    //
    // A pull moves the strip at most one page from its home page. On release
    // it slides on if pulled far enough, and home otherwise.

    property bool pulling: false
    // The page the strip rested on or was sliding to when the pull began. A
    // touchpad's movement often comes in several pulls, and taking the
    // nearest page instead would slide back to a page just left.
    property int pullHomePage: 0
    property real pullStartPosition: 0

    // As a share of the screen's width. The compositor does not report
    // finger speed, so only distance counts.
    readonly property real pullShareThatTurns: 0.12

    function pullBy(pixels) {
        if (!pulling) {
            pullHomePage = slide.running ? slide.to : page
            slide.stop()
            pulling = true
            pullStartPosition = stripPosition
        }
        const wanted = stripPosition - pixels / content.width
        const least = Math.max(0, pullHomePage - 1)
        const most = Math.min(pageCount - 1, pullHomePage + 1)
        stripPosition = Math.max(least, Math.min(most, wanted))
    }

    function endPull() {
        if (!pulling) {
            return
        }
        pulling = false
        const pulled = stripPosition - pullStartPosition
        if (Math.abs(pulled) < pullShareThatTurns) {
            slideToPage(pullHomePage)
            return
        }
        slideToPage(pulled > 0 ? Math.ceil(stripPosition) : Math.floor(stripPosition))
    }

    // A touchpad does not always say when the fingers have left it; a pull
    // without news for this long is taken as over.
    Timer {
        id: pullQuiet
        interval: 150
        onTriggered: grid.endPull()
    }

    function scrolled(wheel) {
        // One notch of a mouse's wheel is 120 units of angle and comes
        // without a beginning or an end.
        const notchUnits = 120
        const angle = Math.abs(wheel.angleDelta.x) > Math.abs(wheel.angleDelta.y)
            ? wheel.angleDelta.x : wheel.angleDelta.y
        const fromWheel = wheel.phase === Qt.NoScrollPhase && Math.abs(angle) >= notchUnits
        const hasDistance = wheel.pixelDelta.x !== 0 || wheel.pixelDelta.y !== 0
        if (fromWheel || !hasDistance) {
            if (angle !== 0) {
                turnPage(angle < 0 ? 1 : -1)
            }
            return
        }

        // A touchpad: the strip follows the fingers along their main direction.
        const distance = Math.abs(wheel.pixelDelta.x) > Math.abs(wheel.pixelDelta.y)
            ? wheel.pixelDelta.x : wheel.pixelDelta.y
        pullBy(distance)
        if (wheel.phase === Qt.ScrollEnd) {
            pullQuiet.stop()
            endPull()
        } else {
            pullQuiet.restart()
        }
    }

    Item {
        id: content
        anchors.fill: parent
        opacity: grid.open ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: grid.open ? grid.openDuration : grid.closeDuration }
        }

        Fill {
            id: backdrop
            anchors.fill: parent
            fill: grid.desktopFill
            themeDirectory: grid.themeDirectory
            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: (grid.looks.background_blur ?? 0) > 0
                blur: 1
                // The theme gives the blur as a Gaussian's sigma in pixels;
                // Qt's blurMax is a radius, about three sigmas.
                blurMax: 3 * (grid.looks.background_blur ?? 0)
            }
        }
        Rectangle {
            anchors.fill: parent
            color: ThemeValues.color(grid.looks.background_veil ?? "#00000000")
        }

        // A press on empty space closes; a drag pulls the pages; the wheel
        // and the touchpad turn them.
        MouseArea {
            anchors.fill: parent

            property real lastX: 0
            property real pressedX: 0
            property bool dragging: false
            readonly property real dragStartDistance: 8

            onPressed: mouse => {
                pressedX = mouse.x
                lastX = mouse.x
                dragging = false
            }
            onPositionChanged: mouse => {
                if (!pressed) {
                    return
                }
                if (!dragging && Math.abs(mouse.x - pressedX) < dragStartDistance) {
                    return
                }
                dragging = true
                grid.pullBy(mouse.x - lastX)
                lastX = mouse.x
            }
            onReleased: {
                if (dragging) {
                    grid.endPull()
                } else {
                    grid.close()
                }
            }
            onCanceled: grid.endPull()
            onWheel: wheel => grid.scrolled(wheel)
        }

        // Everything but the backdrop: with "zoom", it settles into place
        // from a little larger as it fades in, as Launchpad's icons do, and
        // grows again as it fades out.
        Item {
            id: foreground
            anchors.fill: parent
            scale: grid.open || grid.motion.kind !== "zoom" ? 1 : grid.awayScale
            Behavior on scale {
                NumberAnimation {
                    duration: grid.open ? grid.openDuration : grid.closeDuration
                    easing.type: grid.open ? Easing.OutQuad : Easing.InQuad
                }
            }

            // --- The icons ----------------------------------------------------

            // Every application has its place on the strip, which is moved so
            // that the shown page is on the screen.
            Item {
                id: strip
                x: -Math.round(grid.stripPosition * content.width)
                width: content.width
                height: content.height

                Repeater {
                    id: cells
                    model: grid.applications

                    Item {
                        id: cell

                        required property var modelData
                        required property int index

                        readonly property int onPage: Math.floor(index / grid.perPage)
                        readonly property int placeOnPage: index % grid.perPage
                        readonly property int column: placeOnPage % grid.columns
                        readonly property int row: Math.floor(placeOnPage / grid.columns)
                        readonly property real iconSize: grid.looks.icon_size ?? 64

                        // Columns are spread evenly about the page's middle.
                        width: grid.looks.column_pitch ?? 160
                        height: grid.looks.row_pitch ?? 120
                        x: onPage * content.width
                           + Math.round(content.width / 2 + (column - grid.columns / 2) * width)
                           + (grid.looks.columns_shift ?? 0)
                        y: (grid.looks.first_row_top ?? 80) + row * height
                        visible: Math.abs(onPage - grid.stripPosition) < 1

                        Image {
                            id: icon
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: cell.iconSize
                            height: cell.iconSize
                            sourceSize: Qt.size(width, height)
                            source: grid.iconFor(cell.modelData)
                            opacity: press.dragging ? 0.3 : (press.pressed ? 0.6 : 1)
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 8
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            // Placed by its baseline.
                            y: (grid.looks.label_baseline ?? (cell.iconSize + 18)) - baselineOffset
                            text: cell.modelData.name
                            color: ThemeValues.color(grid.looks.label_color ?? "#ffffff")
                            font.family: grid.theme.fonts.app_grid_label.family
                            font.pixelSize: grid.theme.fonts.app_grid_label.size
                            font.bold: grid.theme.fonts.app_grid_label.bold ?? false
                            style: grid.looks.label_shadow_color !== undefined ? Text.Raised : Text.Normal
                            styleColor: ThemeValues.color(grid.looks.label_shadow_color ?? "#00000000")
                        }

                        MouseArea {
                            id: press
                            // The icon and its name, not the whole cell: between
                            // icons a press means "close".
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: cell.iconSize + 24
                            height: (grid.looks.label_baseline ?? (cell.iconSize + 18)) + 6

                            readonly property real dragStartDistance: 8
                            property real pressedX: 0
                            property real pressedY: 0
                            readonly property bool dragging: grid.draggedEntry === cell.modelData
                            // The press that ended a drag is not a click.
                            property bool wasDragged: false

                            onPressed: mouse => {
                                pressedX = mouse.x
                                pressedY = mouse.y
                                wasDragged = false
                                if (mouse.button === Qt.RightButton) {
                                    const point = mapToItem(null, mouse.x, mouse.y)
                                    grid.openMenu(cell.modelData, point.x, point.y)
                                }
                            }
                            onPositionChanged: mouse => {
                                if (!pressed) {
                                    return
                                }
                                if (!dragging && Math.hypot(mouse.x - pressedX, mouse.y - pressedY) < dragStartDistance) {
                                    return
                                }
                                grid.draggedEntry = cell.modelData
                                wasDragged = true
                                // The grid's window covers the screen, so a
                                // place in it is a place on the screen.
                                const point = mapToItem(null, mouse.x, mouse.y)
                                grid.dragMoved(cell.modelData, icon.source, point.x, point.y)
                            }
                            onReleased: {
                                if (dragging) {
                                    grid.draggedEntry = null
                                    grid.dragEnded()
                                }
                            }
                            onCanceled: {
                                if (dragging) {
                                    grid.draggedEntry = null
                                    grid.dragGivenUp()
                                }
                            }
                            onClicked: mouse => {
                                if (wasDragged || mouse.button === Qt.RightButton) {
                                    return
                                }
                                cell.modelData.execute()
                                grid.close()
                            }
                        }
                    }
                }
            }

            // --- The search field ---------------------------------------------

            Item {
                id: searchBox
                width: grid.searchLooks.width ?? 240
                height: grid.searchLooks.height ?? 28
                x: Math.round((content.width - width) / 2)
                y: grid.searchLooks.top ?? 24

                Fill {
                    anchors.fill: parent
                    fill: grid.searchLooks.background ?? ({ color: "#00000040", radius: 6 })
                    themeDirectory: grid.themeDirectory
                }
                Image {
                    readonly property var inset: grid.searchLooks.icon_inset ?? [8, 6]
                    visible: grid.searchLooks.icon !== undefined
                    x: inset[0]
                    y: inset[1]
                    source: grid.searchLooks.icon !== undefined
                        ? "file://" + grid.themeDirectory + "/" + grid.searchLooks.icon : ""
                }
                TextInput {
                    id: searchField
                    x: grid.searchLooks.text_left ?? 10
                    width: parent.width - x - 10
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    focus: true
                    color: ThemeValues.color(grid.searchLooks.text_color ?? "#ffffff")
                    font.family: grid.theme.fonts.interface.family
                    font.pixelSize: grid.theme.fonts.interface.size
                    onTextChanged: grid.jumpToPage(0)

                    // The field has the keyboard for as long as the grid is
                    // up, so the grid's own keys are handled here.
                    Keys.onEscapePressed: {
                        if (text !== "") {
                            text = ""
                        } else {
                            grid.close()
                        }
                    }
                    // Return starts the only application left.
                    onAccepted: {
                        if (grid.applications.length === 1) {
                            grid.applications[0].execute()
                            grid.close()
                        }
                    }
                    Keys.onPressed: event => {
                        // With text typed, the arrow keys move in the text.
                        if (text !== "") {
                            return
                        }
                        if (event.key === Qt.Key_Left) {
                            grid.turnPage(-1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Right) {
                            grid.turnPage(1)
                            event.accepted = true
                        }
                    }
                }
                // Keeps a press in the field from closing the grid.
                MouseArea {
                    anchors.fill: parent
                    onClicked: searchField.forceActiveFocus()
                }
            }

            // --- The page dots --------------------------------------------------

            Row {
                visible: grid.pageCount > 1
                readonly property real dotSize: grid.dotLooks.size ?? 8
                spacing: (grid.dotLooks.pitch ?? 16) - dotSize
                x: Math.round((content.width - width) / 2)
                y: content.height - (grid.dotLooks.center_above_bottom ?? 40) - dotSize / 2

                Repeater {
                    model: grid.pageCount

                    Rectangle {
                        required property int index
                        width: parent.dotSize
                        height: parent.dotSize
                        radius: width / 2
                        color: ThemeValues.color(index === grid.page
                            ? (grid.dotLooks.color ?? "#ffffff")
                            : (grid.dotLooks.other_color ?? "#ffffff50"))

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -4
                            onClicked: grid.slideToPage(parent.index)
                        }
                    }
                }
            }
        }
    }

    onOpenChanged: {
        if (open) {
            searchField.forceActiveFocus()
        }
    }

    // --- A program's menu (secondary click) ---------------------------------

    readonly property var words: (settings.config.app_grid ?? ({}))
    property var menuEntry: null
    readonly property int openId: 1
    readonly property int keepId: 2

    function openMenu(entry, screenX, screenY) {
        menuEntry = entry
        programMenu.menuX = screenX
        programMenu.menuY = screenY
        const items = [
            { id: openId, label: words.open_label ?? "Open", enabled: true, separator: false,
              checkable: false, checked: false, shortcut: [], children: [] }
        ]
        if (dock !== null) {
            items.push({ id: 0, label: "", enabled: false, separator: true,
                         checkable: false, checked: false, shortcut: [], children: [] })
            items.push({ id: keepId, label: words.keep_in_dock_label ?? "Keep in Dock", enabled: true,
                         separator: false, checkable: true, checked: dock.keeps(entry.id), shortcut: [], children: [] })
        }
        programMenu.items = items
        programMenu.open = true
    }

    MenuOverlay {
        id: programMenu
        screen: grid.screen
        settings: grid.settings
        onDismissed: open = false
        onChosen: id => {
            open = false
            const entry = grid.menuEntry
            if (entry === null) {
                return
            }
            if (id === grid.openId) {
                entry.execute()
                grid.close()
            } else if (id === grid.keepId) {
                grid.dock.setKept(entry.id, !grid.dock.keeps(entry.id))
            }
        }
    }
}
