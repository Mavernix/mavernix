// The dock: application icons on a shelf at the bottom of the screen, with
// shadows, reflections, running lights and separators between groups, and a
// last group for minimised windows and the trash. Looks come from the theme's
// [dock] tables, contents and behaviour from the config's [dock] table.
// DockLayout.js counts up from the screen's bottom; `fromBottom()` gives QML's y.

import QtQuick
import Quickshell
import Quickshell.Wayland
import Shell.Native
import "DockLayout.js" as DockLayout
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: dock

    IconSource {
        id: themeIcons
        choices: dock.choices
    }

    required property var settings

    // The shell's root, which a tile can ask to `run(action)`.
    property var shell: null
    // While raised the dock is above everything, the application grid included.
    property bool raised: false
    WlrLayershell.layer: raised ? WlrLayer.Overlay : WlrLayer.Top
    // Lets the compositor tell this surface from the shell's others.
    WlrLayershell.namespace: "mavernix-dock"

    required property var choices
    property var confirmDialog: null
    property var sounds: null

    readonly property var looks: settings.theme.dock
    readonly property var config: settings.config.dock
    readonly property string themeDirectory: settings.theme_directory

    // --- What is in the dock ------------------------------------------------

    // The config's groups of desktop entry IDs, or the user's own once a program
    // has been kept or removed by hand. Running programs in no group join the first.
    readonly property var keptGroups: choices.value("dock", "groups") ?? []

    // --- Dragging a tile ---------------------------------------------------------
    // Tiles can be dragged along the dock or out of it.

    // What is being dragged: { listedAs, iconSource, fromOutside }, or null.
    property var dragged: null
    // Where it would land, counted among all kept tiles along the dock.
    property int dragLanding: 0
    property bool draggedOut: false
    property real dragScreenX: 0
    property real dragScreenY: 0

    // How far above the resting dock a tile must be pulled to leave it, in tile sizes.
    readonly property real outDistanceInTiles: 1

    function entryText(entry) {
        return JSON.stringify(entry)
    }

    readonly property var shownGroups: {
        if (dragged === null) {
            return keptGroups
        }
        const wanted = entryText(dragged.listedAs)
        const groups = keptGroups.map(group => group.filter(entry => entryText(entry) !== wanted))
        if (draggedOut) {
            return groups
        }
        if (groups.length === 0) {
            groups.push([])
        }
        // At the seam between two groups the tile joins the earlier one.
        let left = dragLanding
        for (let number = 0; number < groups.length; number++) {
            const isLast = number === groups.length - 1
            if (left <= groups[number].length || isLast) {
                groups[number].splice(Math.min(left, groups[number].length), 0, dragged.listedAs)
                break
            }
            left -= groups[number].length
        }
        return groups
    }

    function dragTo(index, windowX, windowY) {
        if (dragged === null) {
            dragged = { listedAs: places[index].listedAs, iconSource: tiles.itemAt(index).iconSource,
                        fromOutside: false }
        }
        followDrag(windowX, windowY)
    }

    // Something dragged in from elsewhere, such as a program from the application grid.
    function dragFromOutside(listedAs, iconSource, screenX, screenY) {
        if (dragged === null) {
            dragged = { listedAs: listedAs, iconSource: iconSource, fromOutside: true }
        }
        followDrag(screenX, screenY - (screen.height - height))
    }

    function followDrag(windowX, windowY) {
        dragScreenX = windowX
        dragScreenY = screen.height - height + windowY

        const dockTop = height - restingHeight
        draggedOut = windowY < dockTop - outDistanceInTiles * tileSize

        // Count the other kept tiles whose resting middle is left of the pointer.
        const wanted = entryText(dragged.listedAs)
        let before = 0
        for (let number = 0; number < places.length; number++) {
            const other = places[number]
            if (other.isSeparator || !other.kept || entryText(other.listedAs) === wanted) {
                continue
            }
            const resting = row.tiles[number]
            if (resting.left + resting.width / 2 < windowX) {
                before += 1
            }
        }
        dragLanding = before
    }

    function giveUpDrag() {
        dragged = null
        draggedOut = false
    }

    function endDrag() {
        if (dragged === null) {
            return
        }
        const result = shownGroups
        const wasKept = keptGroups.some(group => group.some(
            entry => entryText(entry) === entryText(dragged.listedAs)))
        const out = draggedOut
        // Something from elsewhere let go away from the dock: nothing changes.
        if (out && dragged.fromOutside) {
            giveUpDrag()
            return
        }
        // Start the removal effect before the dragged icon goes, so that the
        // overlay's window is not taken away in between.
        if (out && wasKept) {
            dragOverlay.showRemoval(dragScreenX, dragScreenY)
            if (sounds !== null) {
                sounds.play("dock-item-remove")
            }
        }
        dragged = null
        draggedOut = false
        // A tile that was there only because its program runs changes nothing.
        if (out && !wasKept) {
            return
        }
        choices.set("dock", "groups", result)
    }

    DockDrag {
        id: dragOverlay
        screen: dock.screen
        looks: dock.looks
        themeDirectory: dock.themeDirectory
        iconSource: dock.dragged?.iconSource ?? ""
        iconSize: dock.tileSize - 2 * dock.tileMargin
        pointX: dock.dragScreenX
        pointY: dock.dragScreenY
    }

    readonly property var places: {
        const installed = DesktopEntries.applications.values

        // `kept` tells a place kept in the dock from one there only while its
        // program runs; `listedAs` is how the groups name it.
        const groups = []
        const listed = []
        for (const ids of shownGroups) {
            const group = []
            for (const id of ids) {
                // A table instead of an ID is a custom tile of the config's making.
                if (typeof id !== "string") {
                    group.push({ isSeparator: false, entry: null, window: null, isTrash: false,
                                 custom: id, kept: true, listedAs: id })
                    continue
                }
                const entry = installed.find(candidate => candidate.id === id)
                if (entry !== undefined) {
                    group.push({ isSeparator: false, entry: entry, window: null, isTrash: false,
                                 kept: true, listedAs: id })
                    listed.push(entry)
                }
            }
            groups.push(group)
        }
        if (groups.length === 0) {
            groups.push([])
        }

        for (const window of ToplevelManager.toplevels.values) {
            const entry = DesktopEntries.heuristicLookup(window.appId)
            if (entry !== null && !listed.includes(entry)) {
                groups[0].push({ isSeparator: false, entry: entry, window: null, isTrash: false,
                                 kept: false, listedAs: entry.id })
                listed.push(entry)
            }
        }

        // last_group_at_end: the config's last group stands with the minimised
        // windows and the trash.
        const endGroup = (config.last_group_at_end ?? false) && groups.length > 1 ? groups.pop() : []

        const result = []
        for (const group of groups) {
            if (group.length === 0) {
                continue
            }
            if (result.length > 0) {
                result.push({ isSeparator: true, entry: null, window: null, isTrash: false })
            }
            for (const place of group) {
                result.push(place)
            }
        }

        const lastGroup = endGroup
        for (const window of minimizedWindows) {
            lastGroup.push({
                isSeparator: false,
                entry: DesktopEntries.heuristicLookup(window.appId),
                window: window,
                isTrash: false
            })
        }
        if (showsTrash) {
            lastGroup.push({ isSeparator: false, entry: null, window: null, isTrash: true })
        }
        if (lastGroup.length > 0 && result.length > 0) {
            result.push({ isSeparator: true, entry: null, window: null, isTrash: false })
        }
        // Each place gets a stable key, so that the model keeps a place's tile
        // when the row changes and only a really new place gets a new tile.
        const all = result.concat(lastGroup)
        let separatorCount = 0
        for (const place of all) {
            if (place.isSeparator) {
                place.key = "separator " + separatorCount
                separatorCount += 1
            } else if (place.isTrash) {
                place.key = "trash"
            } else if (place.window !== null) {
                place.key = "window " + place.window.appId + " " + place.window.title
                    + " " + sameNamedBefore(place.window)
            } else if ((place.custom ?? null) !== null) {
                place.key = "custom " + JSON.stringify(place.custom)
            } else {
                place.key = "entry " + place.entry.id
            }
        }
        return all
    }

    readonly property int endGroupSize:
        (config.last_group_at_end ?? false) && keptGroups.length > 1
            ? keptGroups[keptGroups.length - 1].length : 0

    readonly property var minimizedWindows:
        ToplevelManager.toplevels.values.filter(window => window.minimized)

    // --- Minimised windows' pictures ---------------------------------------------
    // With dock.minimized.shows = "picture", a minimised window's tile shows a
    // small picture of the window, with its icon in the corner.

    readonly property var minimizedLooks: looks.minimized ?? ({})
    readonly property bool showsWindowPictures: (minimizedLooks.shows ?? "icon") === "picture"

    WindowPictures {
        id: windowPictures
        folder: Quickshell.env("XDG_RUNTIME_DIR") + "/mavernix/window-pictures"
        // Large enough for the largest icon in device pixels.
        side: Math.round(dock.largestIconSize * (dock.screen?.devicePixelRatio ?? 1))
        badgeShare: dock.minimizedLooks.badge_share ?? 0
        // Taken once the minimise effect is over.
        delayMs: dock.settings.theme.animation?.minimize?.duration_ms ?? 0
    }

    // Tells apart windows with the same application and title.
    function sameNamedBefore(window) {
        const sameNamed = ToplevelManager.toplevels.values.filter(
            other => other.appId === window.appId && other.title === window.title)
        return Math.max(0, sameNamed.indexOf(window))
    }

    function windowPicture(window, entry) {
        // Read so that bindings follow the pictures.
        windowPictures.revision
        return windowPictures.tileFor(window.appId, window.title, sameNamedBefore(window),
                                      themeIcons.nameFor(entry))
    }
    // Brings a minimised window back. The native module switches to its
    // desktop first if needed and restores it when the slide is over.
    function bringBack(window) {
        const slideMs = ((settings.theme.animation ?? ({})).desktop_switch ?? ({})).duration_ms ?? 0
        // false: the compositor does not know the window.
        const taken = windowPictures.bringBack(window.appId, window.title, sameNamedBefore(window), slideMs)
        if (!taken) {
            window.minimized = false
            window.activate()
        }
    }

    function windowPictureComing(window) {
        windowPictures.revision
        return windowPictures.isComing(window.appId, window.title, sameNamedBefore(window))
    }

    // The theme's own picture for an icon name (dock.icons), else the icon theme's.
    function iconPicture(name) {
        const own = (looks.icons ?? ({}))[name]
        if (own !== undefined) {
            return "file://" + themeDirectory + "/" + own
        }
        return themeIcons.path(name)
    }

    // --- Things dropped onto the dock ----------------------------------------------
    // Dropped folders and files are kept in the dock's last group, beside the trash.

    FileKind {
        id: fileKinds
    }

    function droppedEntry(path) {
        const trimmed = path.endsWith("/") ? path.slice(0, -1) : path
        const name = trimmed.substring(trimmed.lastIndexOf("/") + 1)
        // Home paths are written with "~", as the config writes them.
        const home = Quickshell.env("HOME")
        const written = trimmed.startsWith(home + "/") ? "~" + trimmed.substring(home.length) : trimmed

        if (fileKinds.isFolder(trimmed)) {
            return { name: name, icon: "folder", folder: written,
                     view: config.new_folder_view ?? "grid", order: "name" }
        }
        const icons = fileKinds.iconsFor(trimmed)
        const icon = themeIcons.find(icons.icon) !== "" ? icons.icon : icons.genericIcon
        return { name: name, icon: icon,
                 command: (config.open_command ?? ["xdg-open"]).concat([trimmed]) }
    }

    // Empties the trash with [dock] trash_empty, asking first unless the user
    // turned that off (dock.confirm_empty_trash).
    function emptyTrash() {
        if (config.trash_empty === undefined || trashIsEmpty) {
            return
        }
        const asksFirst = choices.value("dock", "confirm_empty_trash") ?? true
        if (confirmDialog !== null && asksFirst) {
            confirmDialog.run(config.trash_empty)
        } else {
            Quickshell.execDetached(config.trash_empty.command)
        }
    }

    function keepDropped(addresses) {
        const paths = []
        for (const address of addresses) {
            const text = String(address)
            if (text.startsWith("file://")) {
                paths.push(decodeURIComponent(text.substring("file://".length)))
            }
        }
        keepPaths(paths)
    }

    // Keeps files and folders in the last group, skipping those already there.
    // Other programs ask for this through shell.qml ("dock").
    function keepPaths(paths) {
        const groups = keptGroups.map(group => group.slice())
        if (groups.length < 2) {
            groups.push([])
        }
        const lastGroup = groups[groups.length - 1]

        let added = false
        for (const path of paths) {
            const entry = droppedEntry(path)
            if (!lastGroup.some(kept => entryText(kept) === entryText(entry))) {
                lastGroup.push(entry)
                added = true
            }
        }
        if (added) {
            choices.set("dock", "groups", groups)
        }
    }

    // The dropped files as "file://" addresses: from text/uri-list, else from
    // plain-text paths. The uri-list can come back empty: Qt asks a desktop
    // service for the files, and gets nothing where it is not running.
    function droppedAddresses(drop) {
        const lines = text => text.split(/\r?\n/).filter(line => line !== "" && !line.startsWith("#"))
        const listed = lines(drop.getDataAsString("text/uri-list"))
        if (listed.length > 0) {
            return listed
        }
        const addresses = []
        for (const line of lines(drop.getDataAsString("text/plain"))) {
            if (line.startsWith("file://")) {
                addresses.push(line)
            } else if (line.startsWith("/")) {
                addresses.push("file://" + encodeURI(line))
            }
        }
        return addresses
    }

    DropArea {
        anchors.fill: parent
        // No `keys`: with them a drag from another program is never let in.
        onDropped: drop => {
            const addresses = dock.droppedAddresses(drop)
            if (addresses.length === 0) {
                return
            }
            // Onto the trash, files go there; anywhere else they are kept.
            if (dock.overTrash(drop.x)) {
                dock.trashDropped(addresses)
                drop.accept(Qt.MoveAction)
            } else {
                dock.keepDropped(addresses)
                drop.accept(Qt.LinkAction)
            }
        }
    }

    function overTrash(x) {
        const index = places.findIndex(place => place.isTrash)
        const tile = index >= 0 ? tileAt(index) : null
        if (tile === null) {
            return false
        }
        const left = tile.mapToItem(null, 0, 0).x
        return x >= left && x <= left + tile.width
    }

    // Moves dropped files to the trash with [dock] trash_drop_command, which
    // gets the paths appended; does nothing without it.
    function trashDropped(addresses) {
        const command = config.trash_drop_command ?? []
        const paths = []
        for (const address of addresses) {
            const text = String(address)
            if (text.startsWith("file://")) {
                paths.push(decodeURIComponent(text.substring("file://".length)))
            }
        }
        if (command.length > 0 && paths.length > 0) {
            Quickshell.execDetached(command.concat(paths))
        }
    }

    // --- The trash ----------------------------------------------------------------
    readonly property bool showsTrash: config.trash ?? false

    readonly property string trashFolder:
        (Quickshell.env("XDG_DATA_HOME") || (Quickshell.env("HOME") + "/.local/share")) + "/Trash/files"
    FolderWatch {
        id: trashContents
        path: dock.trashFolder
    }
    readonly property bool trashIsEmpty: trashContents.empty
    onTrashIsEmptyChanged: {
        if (trashIsEmpty && settled && sounds !== null) {
            sounds.play("trash-empty")
        }
    }

    readonly property string trashPicture: {
        const looks = dock.looks.trash ?? {}
        const image = trashIsEmpty ? looks.empty_image : looks.full_image
        if (image !== undefined) {
            return "file://" + themeDirectory + "/" + image
        }
        return themeIcons.path(trashIsEmpty ? (looks.empty_icon ?? "user-trash")
                                                : (looks.full_icon ?? "user-trash-full"))
    }

    readonly property var placeWidths: places.map(
        place => place.isSeparator ? looks.separator.width : 1)
    readonly property real rowUnits: placeWidths.reduce((sum, width) => sum + width, 0)

    // --- Telling the compositor where windows minimise to ----------------------
    // The minimise effect needs each window's target rectangle on the dock: its
    // tile's icon, or for a window not minimised, where its new tile would appear.

    // The row with one more minimised window: its widths and the new tile's
    // index. A separator comes with it if the last group does not exist yet.
    readonly property var rowWithOneMore: {
        const widths = placeWidths.slice()
        const lastGroupExists = minimizedWindows.length > 0 || showsTrash || endGroupSize > 0
        const insertAt = showsTrash ? widths.length - 1 : widths.length
        if (!lastGroupExists && widths.length > 0) {
            widths.push(looks.separator.width)
            widths.push(1)
            return { widths: widths, newTile: widths.length - 1 }
        }
        widths.splice(insertAt, 0, 1)
        return { widths: widths, newTile: insertAt }
    }

    function iconRectangle(restingTile) {
        const left = Math.floor(restingTile.left + 0.5)
        const size = Math.floor(restingTile.width + 0.5) - 2 * tileMargin
        return Qt.rect(left + tileMargin, fromBottom(base, size), size, size)
    }

    function tellWindowPlaces() {
        const units = rowWithOneMore.widths.reduce((sum, width) => sum + width, 0)
        const futureTileSize = DockLayout.fittingTileSize(looks, wantedTileSize, units, width)
        const futureRow = DockLayout.row(looks, rowWithOneMore.widths, futureTileSize, width)
        const futureTile = futureRow.tiles[rowWithOneMore.newTile]

        for (const window of ToplevelManager.toplevels.values) {
            const index = places.findIndex(place => place.window === window)
            const tile = index >= 0 ? row.tiles[index] : futureTile
            if (tile === undefined) {
                continue
            }
            // Before the dock has its width a tile can be narrower than its margins.
            // A negative rectangle is a protocol error and the compositor disconnects
            // the shell, so it is skipped; the row changes again once the width is known.
            const rectangle = iconRectangle(tile)
            if (rectangle.width <= 0 || rectangle.height <= 0) {
                continue
            }
            window.setRectangle(dock, rectangle)
        }
    }

    readonly property int windowCount: ToplevelManager.toplevels.values.length
    onRowChanged: tellWindowPlaces()
    onWindowCountChanged: tellWindowPlaces()

    // --- Sizes ------------------------------------------------------------------

    readonly property int tileMargin: looks.tile_margin

    // The icon size: the user's (from dragging a separator), else the config's
    // or theme's. draggedIconSize is the size during such a drag, 0 otherwise.
    property real draggedIconSize: 0
    readonly property var sizeWishes: {
        const chosen = draggedIconSize > 0 ? draggedIconSize : choices.value("dock", "icon_size")
        return chosen !== undefined ? { icon_size: chosen } : {}
    }
    readonly property real smallestIconSize: looks.smallest_icon_size ?? 16
    readonly property real largestIconSize: looks.largest_icon_size

    function resizeTo(iconSize) {
        if (draggedIconSize === 0) {
            heldRestingHeight = restingHeight
        }
        draggedIconSize = Math.max(smallestIconSize, Math.min(largestIconSize, Math.round(iconSize)))
    }
    function endResize() {
        if (draggedIconSize > 0) {
            choices.set("dock", "icon_size", draggedIconSize)
            draggedIconSize = 0
        }
    }

    readonly property real wantedTileSize:
        DockLayout.iconSize(looks, sizeWishes, screen.height) + 2 * tileMargin
    readonly property real tileSize:
        DockLayout.fittingTileSize(looks, wantedTileSize, rowUnits, width)

    // Space between the screen's edge and the line the tiles stand on.
    readonly property int base: DockLayout.sized(looks.base, tileSize)
    readonly property int shelfHeight:
        Math.trunc(base + looks.shelf.height_above_base[0] * tileSize + looks.shelf.height_above_base[1])
    readonly property bool shelfIsSlab: looks.shelf.shape === "slab"

    // --- Magnification ----------------------------------------------------------

    readonly property bool magnificationWanted: choices.value("dock", "magnification") ?? false
    readonly property bool magnifies: magnificationWanted && largestTileSize > tileSize
    readonly property real largestTileSize:
        (choices.value("dock", "largest_icon_size") ?? looks.largest_icon_size) + 2 * tileMargin

    // 0 to 1. Ramps over a + b * ln(1 + change / 2) ms, with [a, b] the theme's
    // magnification_ramp_ms and change the pointed tile's size change in px.
    property real magnificationRamp: 0
    readonly property real magnificationTarget:
        (magnifies && draggedIconSize === 0 && (pointer.hovered || menuHoldsPointer)) ? 1 : 0
    onMagnificationTargetChanged: {
        const sizeNow = tileSize + (largestTileSize - tileSize) * magnificationRamp
        const sizeThen = tileSize + (largestTileSize - tileSize) * magnificationTarget
        const timing = looks.magnification_ramp_ms
        rampAnimation.duration = Math.floor(timing[0] + timing[1] * Math.log(1 + 0.5 * Math.abs(sizeThen - sizeNow)))
        rampAnimation.to = magnificationTarget
        rampAnimation.restart()
    }
    NumberAnimation {
        id: rampAnimation
        target: dock
        property: "magnificationRamp"
        easing.type: Easing.InOutSine
    }

    // --- The row ------------------------------------------------------------------
    // `row` is where tiles rest; what is shown slides there when the row
    // changes, and magnification pushes tiles apart around the pointer.

    readonly property var row: DockLayout.row(looks, placeWidths, tileSize, width)

    readonly property var pushNow: DockLayout.pushNow(
        looks, tileSize, row.start, row.end,
        pointerAlongDock, (largestTileSize - tileSize) * magnificationRamp)

    // Nothing slides while the dock is first being put together.
    property bool settled: false
    Timer {
        interval: 1000
        running: true
        onTriggered: dock.settled = true
    }

    // ...nor while resized by hand: sizes and places change together at every
    // step, and sliding places would lag behind the sizes.
    readonly property bool slides: settled && draggedIconSize === 0

    component Slide: NumberAnimation {
        duration: dock.looks.tile_slide_ms
        easing.type: Easing.InOutSine
    }

    property real shownRowStart: row.start
    property real shownRowEnd: row.end
    Behavior on shownRowStart {
        enabled: dock.slides
        Slide {}
    }
    Behavior on shownRowEnd {
        enabled: dock.slides
        Slide {}
    }

    readonly property var shelfEdges:
        DockLayout.shelfEdges(looks, tileSize, shownRowStart, shownRowEnd, pushNow)

    // The model holds the places' keys, not the places: it matches entries by
    // identity, and every row change makes all places anew, which would hand
    // tiles the wrong contents. A tile looks its place up by key (placeOf).
    ScriptModel {
        id: placeModel
        values: dock.places.map(place => place.key)
    }
    readonly property var placesByKey: {
        const byKey = {}
        for (const place of places) {
            byKey[place.key] = place
        }
        return byKey
    }
    readonly property var noPlace: ({ isSeparator: true, entry: null, window: null, isTrash: false })
    function placeOf(key) {
        return placesByKey[key] ?? noPlace
    }

    // The tile at a place, for the other layers. Through tilesRevision they ask
    // again whenever a tile comes or goes: a layer's item may be made first.
    property int tilesRevision: 0
    function tileAt(index) {
        tilesRevision
        return tiles.itemAt(index)
    }

    // --- The window ---------------------------------------------------------------

    anchors {
        left: true
        right: true
        bottom: true
    }

    // Always as high as the tallest tile at the top of its bounce, so resizing
    // by hand never resizes the window: that made the drag uneven, as shell and
    // compositor briefly disagree about the window's top. The window is clear
    // and only the mask takes the pointer, so the spare height costs nothing.
    readonly property real tallestTile: largestIconSize + 2 * tileMargin
    readonly property real bounceRoom: DockLayout.sized(looks.launch_bounce.height, tallestTile) + 1
    readonly property real labelRoom: looks.label !== undefined
        ? looks.label.gap + looks.label.pointer_height + looks.label.height : 0
    // The base line rises a little with tile size; the largest tile's is taken.
    readonly property real roomBase: DockLayout.sized(looks.base, tallestTile)
    implicitHeight: roomBase + tallestTile + Math.max(bounceRoom, labelRoom) + looks.headroom

    // Windows keep clear of the resting dock only, and during a resize by
    // hand they keep the height from its start.
    readonly property real restingHeight: base + tileSize + looks.headroom
    property real heldRestingHeight: 0
    exclusiveZone: hidesItself ? 0 : (draggedIconSize > 0 ? heldRestingHeight : restingHeight)

    // --- Hiding ----------------------------------------------------------------------
    // With [dock] autohide the dock sinks below the screen's edge until the pointer
    // reaches it; waits and motion are the theme's [animation.dock_hide].

    readonly property bool hidesItself: choices.value("dock", "autohide") ?? false
    readonly property var hideMotion: (settings.theme.animation ?? ({})).dock_hide ?? ({})
    // What keeps the dock in view: the pointer, a menu or folder, a drag, a
    // resize, or the shell raising it.
    readonly property bool keptInView: !hidesItself || pointerInside || atScreenEdge.hovered
        || menuIndex >= 0 || folderIndex >= 0 || dragged !== null || draggedIconSize > 0 || raised
    property bool inView: true
    onKeptInViewChanged: {
        comingOrGoing.interval = keptInView ? (hideMotion.show_delay_ms ?? 0) : (hideMotion.hide_delay_ms ?? 0)
        comingOrGoing.restart()
    }
    Timer {
        id: comingOrGoing
        onTriggered: dock.inView = dock.keptInView
    }
    // 0 in view, 1 out of sight.
    property real sunk: inView ? 0 : 1
    Behavior on sunk {
        NumberAnimation {
            duration: dock.hideMotion.duration_ms ?? 0
            easing.type: ThemeValues.easing(dock.hideMotion.curve ?? "ease-in-out")
        }
    }
    // The contents move down, not the window: moving the window by a negative
    // margin goes through the compositor at every step and comes out jerky.
    Binding {
        target: dock.contentItem
        property: "y"
        value: dock.sunk * dock.restingHeight
    }

    Item {
        id: screenEdge
        readonly property real rows: 2
        width: parent.width
        height: rows
        y: parent.height - dock.sunk * dock.restingHeight - rows

        HoverHandler {
            id: atScreenEdge
        }
    }

    // The window spans the screen's width but is clear; only the shelf and icons
    // take the pointer, so whatever is behind the room above stays in reach.
    color: "transparent"
    // A sunk dock takes the pointer only on the screen's last rows.
    mask: Region {
        item: dock.sunk > 0 ? screenEdge : pointerArea
    }

    // The pointer's x in window coordinates, not the watching item's: that item
    // moves with the shelf's end as icons grow.
    readonly property bool pointerInside: pointer.hovered
    readonly property real pointerX: pointer.point.scenePosition.x

    // While a menu is open the pointer's place is held (heldPointer, -1 for none),
    // so an enlarged icon does not shrink away from under its menu.
    property real heldPointer: -1
    readonly property bool menuHoldsPointer: (menuIndex >= 0 || folderIndex >= 0) && heldPointer >= 0
    readonly property real pointerAlongDock: menuHoldsPointer ? heldPointer : pointerX

    function fromBottom(bottom, itemHeight) {
        return height - bottom - itemHeight
    }

    Item {
        id: pointerArea
        x: dock.shelfEdges.left
        width: dock.shelfEdges.right - dock.shelfEdges.left
        height: dock.restingHeight + (dock.largestTileSize - dock.tileSize) * dock.magnificationRamp
        y: parent.height - height
        // Above the tiles so it always hears the pointer; presses still reach them.
        z: 1

        HoverHandler {
            id: pointer
        }

        // Follows and ends a tile's drag. The tile cannot: a non-running program's
        // tile pulled out of the dock loses its place and is destroyed while the
        // button is still held. This only watches; the press still goes to the tile.
        PointHandler {
            id: dragEnd
            acceptedButtons: Qt.LeftButton
            onPointChanged: {
                if (active && dock.dragged !== null && !dock.dragged.fromOutside) {
                    dock.followDrag(point.scenePosition.x, point.scenePosition.y)
                }
            }
            onActiveChanged: {
                if (!active) {
                    dock.endDrag()
                }
            }
        }
    }

    // --- Layers, from the lowest to the highest -----------------------------------

    Repeater {
        model: placeModel
        DockIconEffect {
            drawnWidth: dock.largestIconSize
            required property int index
            required property string modelData
            readonly property var place: dock.placeOf(modelData)
            readonly property var tile: dock.tileAt(index)

            visible: !place.isSeparator && dock.looks.icon_shadow !== undefined
            iconSource: tile?.iconSource ?? ""
            tint: dock.looks.icon_shadow?.color ?? "#00000000"
            opacity: tile?.shownShare ?? 1
            blurRadius: dock.looks.icon_shadow?.blur ?? 0

            x: tile?.iconX ?? 0
            width: tile?.iconSize ?? 0
            height: width
            y: dock.fromBottom(dock.base + (tile?.lift ?? 0) + (dock.looks.icon_shadow?.raise ?? 0), height)
        }
    }

    Loader {
        id: shelf
        x: dock.shelfEdges.left
        width: dock.shelfEdges.right - dock.shelfEdges.left
        height: dock.shelfHeight
        y: dock.fromBottom(0, height)
        sourceComponent: dock.shelfIsSlab ? slabShelf : boxShelf
    }

    Component {
        id: slabShelf
        SlabShelf {
            looks: dock.looks.shelf
        }
    }

    Component {
        id: boxShelf
        Fill {
            fill: dock.looks.shelf.background
            themeDirectory: dock.themeDirectory
        }
    }

    Repeater {
        model: placeModel
        Item {
            id: separator
            required property int index
            required property string modelData
            readonly property var place: dock.placeOf(modelData)
            readonly property var tile: dock.tileAt(index)
            readonly property var looks: dock.looks.separator

            visible: place.isSeparator
            // A separator is the dock's handle: dragging it resizes the icons; a
            // secondary press opens the dock's menu.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.SizeVerCursor

                property real heightAtPress: 0
                property real iconSizeAtPress: 0
                function heightAboveBottom(mouse) {
                    return dock.height - mapToItem(null, mouse.x, mouse.y).y
                }

                onPressed: mouse => {
                    if (mouse.button === Qt.LeftButton) {
                        heightAtPress = heightAboveBottom(mouse)
                        iconSizeAtPress = dock.tileSize - 2 * dock.tileMargin
                    }
                }
                onPositionChanged: mouse => {
                    if (pressedButtons & Qt.LeftButton) {
                        dock.resizeTo(iconSizeAtPress + heightAboveBottom(mouse) - heightAtPress)
                    }
                }
                onReleased: dock.endResize()
                onCanceled: dock.endResize()
                onClicked: mouse => {
                    if (mouse.button === Qt.RightButton) {
                        dock.openMenu(separator.index)
                    }
                }
            }
            height: dock.shelfHeight - looks.bottom - looks.top_gap
            width: tile?.width ?? 0
            x: tile ? Math.floor(tile.x + tile.width / 2 + 0.5) - width / 2 : 0
            y: dock.fromBottom(looks.bottom, height)

            Image {
                anchors.fill: parent
                visible: separator.looks.image !== undefined
                source: visible ? "file://" + dock.themeDirectory + "/" + separator.looks.image : ""
                fillMode: Image.PreserveAspectFit
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: separator.looks.color !== undefined
                width: separator.looks.line_width ?? 1
                height: parent.height
                color: ThemeValues.color(separator.looks.color)
            }
        }
    }

    // Reflections in the shelf, cut off before its front face.
    Item {
        id: reflections
        readonly property var looks: dock.looks.reflection
        visible: looks !== undefined
        clip: true
        x: 0
        width: parent.width
        height: dock.shelfHeight - (looks?.clip_bottom ?? 0)
        y: dock.fromBottom(looks?.clip_bottom ?? 0, height)

        Repeater {
            model: reflections.visible ? placeModel : null
            DockIconEffect {
                drawnWidth: dock.largestIconSize
                required property int index
                required property string modelData
                readonly property var place: dock.placeOf(modelData)
                readonly property var tile: dock.tileAt(index)

                visible: !place.isSeparator
                iconSource: tile?.iconSource ?? ""
                upsideDown: true
                blurRadius: reflections.looks.blur * width
                contrast: reflections.looks.contrast
                brightness: reflections.looks.brightness
                opacity: reflections.looks.opacity * (tile?.shownShare ?? 1)

                x: tile?.iconX ?? 0
                width: tile?.iconSize ?? 0
                height: width
                // Its top edge starts a little above the icon's lower edge,
                // hidden behind the icon, and it moves down as the icon rises.
                readonly property real topAboveScreenEdge:
                    dock.base + reflections.looks.overlap - (tile?.lift ?? 0)
                y: reflections.height + reflections.looks.clip_bottom - topAboveScreenEdge
            }
        }
    }

    Repeater {
        model: dock.looks.floor_shadow !== undefined ? placeModel : null
        DockIconEffect {
            drawnWidth: dock.largestIconSize
            required property int index
            required property string modelData
            readonly property var place: dock.placeOf(modelData)
            readonly property var tile: dock.tileAt(index)
            readonly property real iconSize: tile?.iconSize ?? 0

            visible: !place.isSeparator
            iconSource: tile?.iconSource ?? ""
            tint: dock.looks.floor_shadow.color
            opacity: tile?.shownShare ?? 1
            blurRadius: dock.looks.floor_shadow.blur * iconSize

            x: tile?.iconX ?? 0
            width: iconSize
            height: iconSize * dock.looks.floor_shadow.height
            // Centred a quarter of the icon's height above its lower edge.
            y: dock.fromBottom(dock.base + iconSize / 4 - height / 2, height)
        }
    }

    // Lights under running programs; a theme may give a smaller one for small tiles.
    Repeater {
        model: placeModel
        Fill {
            required property int index
            required property string modelData
            readonly property var place: dock.placeOf(modelData)
            readonly property var tile: dock.tileAt(index)
            readonly property var looks: dock.looks.indicator
            readonly property bool small: looks.small_fill !== undefined
                                          && dock.tileSize < looks.small_below_tile_size

            visible: !place.isSeparator && (tile?.running ?? false)
            fill: small ? looks.small_fill : looks.fill
            themeDirectory: dock.themeDirectory
            width: small ? looks.small_width : looks.width
            height: looks.height

            x: tile ? Math.trunc(tile.x + tile.width / 2 - width / 2) : 0
            y: dock.fromBottom(Math.trunc(looks.center_height - height / 2), height)
        }
    }

    Repeater {
        id: tiles
        model: placeModel
        onItemAdded: dock.tilesRevision += 1
        onItemRemoved: dock.tilesRevision += 1

        DockTile {
            required property int index
            required property string modelData
            readonly property var place: dock.placeOf(modelData)
            readonly property var resting: dock.row.tiles[index]
            property real restLeft: resting?.left ?? 0
            // Slides only once placed, so a new tile does not slide in from the edge.
            property bool placed: false
            Component.onCompleted: Qt.callLater(() => placed = true)
            Behavior on restLeft {
                enabled: dock.slides && placed
                Slide {}
            }
            readonly property var edges:
                DockLayout.tileEdges(restLeft, resting?.width ?? 0, dock.pushNow)

            entry: place.entry
            choices: dock.choices
            minimizedWindow: place.window
            readonly property var custom: place.custom ?? null
            readonly property bool showsOpenFolder:
                index === dock.folderIndex && dock.looks.folder?.open_image !== undefined
            // A minimised window's tile is empty while its picture is being made.
            pictureComing: place.window !== null && dock.showsWindowPictures
                && dock.windowPictureComing(place.window)
            picture: {
                if (place.window !== null && dock.showsWindowPictures) {
                    return dock.windowPicture(place.window, place.entry)
                }
                if (place.isTrash) {
                    return dock.trashPicture
                }
                if (showsOpenFolder) {
                    return "file://" + dock.themeDirectory + "/" + dock.looks.folder.open_image
                }
                return custom !== null ? dock.iconPicture(custom.icon ?? "") : ""
            }
            folder: custom?.folder ?? ""
            onFolderWanted: dock.showFolder(index)
            onMinimizedWindowWanted: window => dock.bringBack(window)
            drawnIconSize: dock.magnifies ? dock.largestIconSize : 0
            name: place.isTrash ? (dock.config.trash_label ?? "") : (custom?.name ?? "")
            command: place.isTrash ? (dock.config.trash_command ?? []) : (custom?.command ?? [])
            shellAction: custom?.shell ?? ""
            onShellActionWanted: action => dock.shell?.run(action)
            onMenuWanted: dock.openMenu(index)
            draggable: place.listedAs !== undefined
            lifted: dock.dragged !== null && place.listedAs !== undefined
                    && dock.entryText(place.listedAs) === dock.entryText(dock.dragged.listedAs)
            onDragMoved: (windowX, windowY) => dock.dragTo(index, windowX, windowY)
            margin: dock.tileMargin
            bounce: dock.looks.launch_bounce
            pressedDarken: dock.looks.pressed_darken
            // A tile that joins a dock already on screen grows into place.
            appearDuration: dock.settled ? dock.looks.tile_appear_ms : 0
            smallestAppearScale: dock.looks.tile_appear_scale ?? 1

            // Not while a menu is open: its place needs no label.
            pointerOver: dock.menuIndex < 0 && dock.folderIndex < 0 && dock.dragged === null
                         && dock.pointerInside
                         && dock.pointerX >= x && dock.pointerX < x + width

            // The last tile pointed at keeps the label while it fades out.
            onHoveredChanged: {
                if (hovered) {
                    dock.labelledTile = this
                }
            }

            x: edges.left
            width: edges.right - edges.left
            height: width
            y: dock.fromBottom(dock.base + lift, height)
        }
    }

    // --- The menu of a place ------------------------------------------------------
    // A secondary press opens a menu above the place. Wording is the config's
    // (dock.words), looks the theme's (dock.menu over the ordinary menu).

    property int menuIndex: -1
    property var menuItems: []

    readonly property var words: config.words ?? ({})
    readonly property var menuLooks: Object.assign({}, settings.theme.menu, looks.menu ?? ({}))

    // Menu entry ids; a program's windows count from windowBaseId.
    readonly property int keepId: 1
    readonly property int removeId: 2
    readonly property int optionsId: 3
    readonly property int openId: 4
    readonly property int quitId: 5
    readonly property int emptyTrashId: 6
    readonly property int magnificationId: 7
    readonly property int openFolderId: 8
    readonly property int settingsId: 9
    readonly property int windowBaseId: 100
    // Folder orders, views and listed files count from these.
    readonly property int orderBaseId: 200
    readonly property int viewBaseId: 300
    readonly property int fileBaseId: 10000

    function menuEntry(id, label, enabled, children) {
        return { id: id, label: label, enabled: enabled ?? true, separator: false,
                 checkable: false, checked: false, shortcut: [], children: children ?? [] }
    }
    function menuSeparator() {
        return { id: -1, label: "", enabled: false, separator: true,
                 checkable: false, checked: false, shortcut: [], children: [] }
    }

    function menuFor(place, tile) {
        if (place.isSeparator) {
            const items = [menuEntry(magnificationId, magnificationWanted
                ? (words.magnification_off ?? "Turn magnification off")
                : (words.magnification_on ?? "Turn magnification on"))]
            if (config.settings_command !== undefined) {
                items.push(menuSeparator())
                items.push(menuEntry(settingsId, words.settings ?? "Dock settings…"))
            }
            return items
        }
        if (place.isTrash) {
            return [
                menuEntry(openId, words.open ?? "Open"),
                menuSeparator(),
                // Nothing to empty, or no command for it: greyed.
                menuEntry(emptyTrashId, words.empty_trash ?? "Empty trash",
                          !trashIsEmpty && config.trash_empty !== undefined)
            ]
        }
        if (place.window !== null) {
            return [menuEntry(openId, (words.open_named ?? "Open “%1”").arg(place.window.title))]
        }
        if (place.custom?.folder !== undefined) {
            return folderMenu(place.custom)
        }

        const items = []
        const windows = tile.windows
        for (let number = 0; number < windows.length; number++) {
            const windowEntry = menuEntry(windowBaseId + number, windows[number].title)
            windowEntry.checkable = true
            windowEntry.checked = windows[number].activated
            items.push(windowEntry)
        }
        if (windows.length > 0) {
            items.push(menuSeparator())
        }

        // A running program can be kept or let go; a kept idle one only removed.
        const options = []
        if (tile.running) {
            const keep = menuEntry(keepId, words.keep ?? "Keep in dock")
            keep.checkable = true
            keep.checked = place.kept
            options.push(keep)
        } else {
            options.push(menuEntry(removeId, words.remove ?? "Remove from dock"))
        }
        items.push(menuEntry(optionsId, words.options ?? "Options", true, options))
        items.push(menuSeparator())
        items.push(tile.running ? menuEntry(quitId, words.quit ?? "Quit")
                                : menuEntry(openId, words.open ?? "Open"))
        return items
    }

    function openMenu(index) {
        showMenu(index, menuFor(places[index], tiles.itemAt(index)))
    }

    function showMenu(index, items) {
        menuItems = items
        heldPointer = pointerInside ? pointerX : -1
        menuIndex = index
    }

    function closeMenu() {
        menuIndex = -1
    }

    function groupsWith(id) {
        const groups = keptGroups.map(group => group.slice())
        if (groups.length === 0) {
            groups.push([])
        }
        groups[0].push(id)
        return groups
    }
    function groupsWithout(listedAs) {
        const wanted = JSON.stringify(listedAs)
        return keptGroups.map(group => group.filter(id => JSON.stringify(id) !== wanted))
    }

    function groupsReplacing(listedAs, replacement) {
        const wanted = JSON.stringify(listedAs)
        return keptGroups.map(group => group.map(id => JSON.stringify(id) === wanted ? replacement : id))
    }

    function menuChosen(id) {
        const place = places[menuIndex]
        const tile = tiles.itemAt(menuIndex)
        closeMenu()

        if (id >= fileBaseId) {
            openFile(folderContents.entries[id - fileBaseId].path)
        } else if (id === openFolderId) {
            openFile(expandedPath(place.custom.folder))
        } else if (id >= viewBaseId) {
            const changed = Object.assign({}, place.custom, { view: folderViews[id - viewBaseId] })
            choices.set("dock", "groups", groupsReplacing(place.listedAs, changed))
        } else if (id >= orderBaseId) {
            const changed = Object.assign({}, place.custom, { order: folderOrders[id - orderBaseId] })
            choices.set("dock", "groups", groupsReplacing(place.listedAs, changed))
        } else if (id === keepId) {
            choices.set("dock", "groups",
                        place.kept ? groupsWithout(place.listedAs) : groupsWith(place.listedAs))
        } else if (id === removeId) {
            choices.set("dock", "groups", groupsWithout(place.listedAs))
            if (sounds !== null) {
                sounds.play("dock-item-remove")
            }
        } else if (id === openId) {
            tile.activate()
        } else if (id === quitId) {
            // Asks each window to close; the program may still ask about unsaved work.
            for (const window of tile.windows) {
                window.close()
            }
        } else if (id === emptyTrashId) {
            emptyTrash()
        } else if (id === magnificationId) {
            choices.set("dock", "magnification", !magnificationWanted)
        } else if (id === settingsId) {
            Quickshell.execDetached(config.settings_command)
        } else if (id >= windowBaseId) {
            tile.windows[id - windowBaseId]?.activate()
        }
    }

    MenuOverlay {
        id: placeMenu
        screen: dock.screen
        settings: dock.settings
        looks: dock.menuLooks
        visible: dock.menuIndex >= 0
        items: dock.menuItems

        // Above its place, centred, its pointer ending just above the icon.
        readonly property var tile: dock.menuIndex >= 0 ? tiles.itemAt(dock.menuIndex) : null
        readonly property real placeMiddle: tile ? tile.x + tile.width / 2 : 0
        readonly property real placeHeight:
            tile && !(dock.places[dock.menuIndex]?.isSeparator ?? false) ? tile.width : dock.tileSize
        readonly property real iconTopAboveBottom: dock.base + placeHeight - dock.tileMargin
        readonly property real pointerLength: dock.menuLooks.pointer?.height ?? 0

        pointerScreenX: Math.round(placeMiddle)
        menuX: Math.round(placeMiddle - menuWidth / 2)
        menuY: screen.height - iconTopAboveBottom - (dock.menuLooks.tile_gap ?? 4)
               - pointerLength - menuHeight

        onChosen: id => dock.menuChosen(id)
        onDismissed: dock.closeMenu()
    }

    // --- Folders in the dock ------------------------------------------------------
    // A custom place with `folder` shows its contents on a press, by its `view`
    // (fan, grid or a list menu); its own menu changes view and order.

    readonly property var folderOrders: ["name", "added", "modified", "created", "kind"]
    readonly property var folderViews: ["fan", "grid", "list"]

    property int folderIndex: -1
    property var listedFolder: null

    function expandedPath(path) {
        return path.startsWith("~") ? Quickshell.env("HOME") + path.substring(1) : path
    }

    FolderContents {
        id: folderContents
        path: dock.listedFolder !== null ? dock.expandedPath(dock.listedFolder.folder) : ""
        order: dock.listedFolder?.order ?? "name"
    }

    function openFile(path) {
        Quickshell.execDetached((config.open_command ?? ["xdg-open"]).concat([path]))
    }

    function showFolder(index) {
        const folder = places[index].custom
        listedFolder = folder
        const view = folder.view ?? "list"
        if (view === "grid" || view === "fan") {
            heldPointer = pointerInside ? pointerX : -1
            folderIndex = index
            return
        }

        const items = folderContents.entries.map(
            (file, number) => menuEntry(fileBaseId + number, file.name))
        if (items.length > 0) {
            items.push(menuSeparator())
        }
        items.push(menuEntry(openFolderId, words.open_in_file_manager ?? "Open in file manager"))
        showMenu(index, items)
    }

    function closeFolder() {
        folderIndex = -1
    }

    function folderMenu(folder) {
        const heading = label => menuEntry(-1, label, false)
        const ticked = (id, label, isChosen) => {
            const entry = menuEntry(id, label)
            entry.checkable = true
            entry.checked = isChosen
            return entry
        }

        const orderWords = [words.sort_name ?? "Name", words.sort_added ?? "Date added",
                            words.sort_modified ?? "Date modified",
                            words.sort_created ?? "Date created", words.sort_kind ?? "Kind"]
        const viewWords = [words.view_fan ?? "Fan", words.view_grid ?? "Grid", words.view_list ?? "List"]

        const items = [heading(words.sort_by ?? "Sort by")]
        folderOrders.forEach((order, number) => items.push(
            ticked(orderBaseId + number, orderWords[number], (folder.order ?? "name") === order)))
        items.push(menuSeparator())
        items.push(heading(words.view_as ?? "Show contents as"))
        folderViews.forEach((view, number) => items.push(
            ticked(viewBaseId + number, viewWords[number], (folder.view ?? "list") === view)))
        items.push(menuSeparator())
        items.push(menuEntry(optionsId, words.options ?? "Options", true,
                             [menuEntry(removeId, words.remove ?? "Remove from dock")]))
        items.push(menuSeparator())
        items.push(menuEntry(openFolderId, (words.open_named ?? "Open “%1”").arg(folder.name ?? "")))
        return items
    }

    FolderOverlay {
        screen: dock.screen
        settings: dock.settings
        plainBoxLooks: dock.menuLooks
        open: dock.folderIndex >= 0
        view: dock.listedFolder?.view ?? "grid"
        title: dock.listedFolder?.name ?? ""
        entries: folderContents.entries
        openLabel: dock.words.open_in_file_manager ?? "Open in file manager"
        moreLabel: dock.words.more_in_file_manager ?? "%1 more"

        readonly property var tile: dock.folderIndex >= 0 ? tiles.itemAt(dock.folderIndex) : null
        // (The last place is kept while they go away.)
        property real lastAnchorX: 0
        property real lastAnchorY: 0
        onTileChanged: {
            if (tile !== null) {
                lastAnchorX = Math.round(tile.x + tile.width / 2)
                lastAnchorY = screen.height - (dock.base + tile.width - dock.tileMargin)
            }
        }
        anchorX: lastAnchorX
        anchorY: lastAnchorY

        onFileChosen: path => {
            dock.closeFolder()
            dock.openFile(path)
        }
        onFolderChosen: {
            dock.closeFolder()
            dock.openFile(dock.expandedPath(dock.listedFolder.folder))
        }
        onDismissed: dock.closeFolder()
    }

    // The name of the tile under the pointer.
    property var labelledTile: null

    DockLabel {
        visible: dock.looks.label !== undefined && opacity > 0
        looks: dock.looks.label ?? ({})
        font: dock.settings.theme.fonts.dock_label ?? ({})
        text: dock.labelledTile?.labelText ?? ""
        // Not during a resize, when tiles pass under the pointer unpointed.
        wanted: (dock.labelledTile?.hovered ?? false) && dock.draggedIconSize === 0

        x: dock.labelledTile
            ? Math.round(dock.labelledTile.x + dock.labelledTile.width / 2 - width / 2) : 0
        y: dock.labelledTile
            ? dock.labelledTile.y - (dock.looks.label?.gap ?? 0) - height : 0
    }
}
