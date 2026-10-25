// The files on the desktop: the user's Desktop folder as icons with their
// names, in columns from the screen's far top corner. Icons can be picked,
// opened, dragged, dropped on and renamed; an icon dragged on the desktop
// stays where it was put.
//
// The shell only shows and picks; whatever is done to a file is done by the
// commands in the config's [desktop_files] (`gio` by default). Looks: the
// theme's [desktop.icons] table; a theme without it shows no files.

import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import Shell.Native
import "ThemeValues.js" as ThemeValues

Item {
    id: icons

    IconSource {
        id: themeIcons
    }

    // The shell's UserChoices keep the chosen order; the screen is for the menu's window.
    required property var settings
    required property var userChoices
    required property var screen

    readonly property var looks: (settings.theme.desktop ?? ({})).icons ?? null
    readonly property var wishes: settings.config.desktop_files ?? ({})
    readonly property var words: wishes.words ?? ({})
    readonly property var fonts: settings.theme.fonts
    readonly property string themeDirectory: settings.theme_directory

    readonly property bool shown: looks !== null && (wishes.show ?? false)
    visible: shown

    // The config's folder, "~" standing for the home folder.
    readonly property string folder: {
        const wanted = wishes.folder ?? "~/Desktop"
        return wanted.startsWith("~") ? Quickshell.env("HOME") + wanted.substring(1) : wanted
    }
    readonly property string order: userChoices.value("desktop_files", "order") ?? "name"

    FolderContents {
        id: contents
        path: icons.shown ? icons.folder : ""
        order: icons.order
    }
    readonly property var entries: contents.entries

    // --- Where the icons go ---------------------------------------------------------

    readonly property real topRoom:
        (settings.config.panel?.position ?? "top") === "top" ? (settings.theme.panel?.height ?? 0) : 0
    readonly property real iconSize: looks?.icon_size ?? 48
    readonly property real rowPitch: looks?.row_pitch ?? 96
    readonly property real columnPitch: looks?.column_pitch ?? 96
    // The rows that fit between the panel and the room kept free at the bottom.
    readonly property int rows: Math.max(1, Math.floor(
        (height - topRoom - (looks?.top ?? 8) - (looks?.bottom_room ?? 100)) / rowPitch))

    // The top left corner of the icon in a slot. Slots are counted down the
    // columns, the first column at the screen's far side.
    function slotX(slot) {
        const column = Math.floor(slot / rows)
        return width - (looks?.edge_to_icon_middle ?? 60) - column * columnPitch - iconSize / 2
    }
    function slotY(slot) {
        return topRoom + (looks?.top ?? 8) + (slot % rows) * rowPitch
    }
    function slotAt(x, y) {
        const column = Math.max(0, Math.round(
            (width - (looks?.edge_to_icon_middle ?? 60) - iconSize / 2 - x) / columnPitch))
        const row = Math.max(0, Math.min(rows - 1, Math.round((y - topRoom - (looks?.top ?? 8)) / rowPitch)))
        return column * rows + row
    }

    // Files put by hand: name -> offset of the icon's left edge from the
    // screen's far side and its top from the screen's top. Measured from the
    // far side so the icons stay beside that edge on other screen sizes.
    readonly property var putByHand: userChoices.value("desktop_files", "places") ?? ({})

    // Whether an icon at (x, y) covers any of a slot: each takes a box one
    // pitch square, centred across on its picture.
    function coversSlot(x, y, slot) {
        const halfGap = (columnPitch - iconSize) / 2
        const left = x - halfGap
        const slotLeft = slotX(slot) - halfGap
        return left < slotLeft + columnPitch && slotLeft < left + columnPitch
            && y < slotY(slot) + rowPitch && slotY(slot) < y + rowPitch
    }

    // Where every file's icon is, in the order of `entries`: where it was put
    // by hand, else the next slot no hand-placed icon covers (one placed
    // between slots can cover up to four).
    readonly property var places: {
        const result = []
        const putPlaces = []
        for (const entry of entries) {
            const put = putByHand[entry.name]
            if (put !== undefined) {
                const place = Qt.point(width - put[0], put[1])
                putPlaces.push(place)
                result.push(place)
            } else {
                result.push(null)
            }
        }
        const taken = {}
        const slotsToCheck = entries.length + 4 * putPlaces.length
        for (let slot = 0; slot < slotsToCheck; slot++) {
            for (const place of putPlaces) {
                if (coversSlot(place.x, place.y, slot)) {
                    taken[slot] = true
                    break
                }
            }
        }
        let slot = 0
        for (let index = 0; index < result.length; index++) {
            if (result[index] !== null) {
                continue
            }
            while (taken[slot]) {
                slot++
            }
            result[index] = Qt.point(slotX(slot), slotY(slot))
            slot++
        }
        return result
    }
    function iconX(index) {
        return places[index]?.x ?? 0
    }
    function iconY(index) {
        return places[index]?.y ?? 0
    }

    // Remembers places put by hand ({ name: Qt.point(left, top) }) and drops
    // files that are gone.
    function rememberPlaces(moved) {
        const next = {}
        for (const entry of entries) {
            const place = moved[entry.name]
            if (place !== undefined) {
                next[entry.name] = [Math.round(width - place.x), Math.round(place.y)]
            } else if (putByHand[entry.name] !== undefined) {
                next[entry.name] = putByHand[entry.name]
            }
        }
        userChoices.set("desktop_files", "places", next)
    }

    // Moves the picked icons so the dragged one has its middle at a place,
    // keeping them on the screen and under the panel.
    function movePicked(draggedPath, middleX, middleY) {
        const draggedIndex = entries.findIndex(entry => entry.path === draggedPath)
        if (draggedIndex < 0) {
            return
        }
        const byX = middleX - iconSize / 2 - iconX(draggedIndex)
        const byY = middleY - iconSize / 2 - iconY(draggedIndex)
        const moved = {}
        for (let index = 0; index < entries.length; index++) {
            if (isPicked(entries[index].path)) {
                moved[entries[index].name] = Qt.point(
                    Math.max(0, Math.min(width - iconSize, iconX(index) + byX)),
                    Math.max(topRoom, Math.min(height - iconSize, iconY(index) + byY)))
            }
        }
        rememberPlaces(moved)
    }

    // Snaps every hand-placed icon to its nearest free slot.
    function cleanUp() {
        const moved = {}
        const taken = {}
        for (let index = 0; index < entries.length; index++) {
            if (putByHand[entries[index].name] === undefined) {
                continue
            }
            let slot = slotAt(iconX(index), iconY(index))
            while (taken[slot]) {
                slot++
            }
            taken[slot] = true
            moved[entries[index].name] = Qt.point(slotX(slot), slotY(slot))
        }
        rememberPlaces(moved)
    }

    // --- What is picked ---------------------------------------------------------------

    property var pickedPaths: []

    function isPicked(path) {
        return pickedPaths.includes(path)
    }
    function pickOnly(path) {
        pickedPaths = [path]
    }
    function togglePicked(path) {
        pickedPaths = isPicked(path) ? pickedPaths.filter(other => other !== path) : pickedPaths.concat([path])
    }
    function pickNone() {
        pickedPaths = []
    }
    onEntriesChanged: {
        const there = entries.map(entry => entry.path)
        pickedPaths = pickedPaths.filter(path => there.includes(path))
    }

    // --- What is done to files ------------------------------------------------------------

    function run(command, more) {
        if ((command ?? []).length > 0) {
            Quickshell.execDetached(command.concat(more))
        }
    }
    // An executable .desktop file is started, with any files given to it.
    function isLauncher(path) {
        return entries.some(entry => entry.path === path && entry.isLauncher)
    }
    function launch(launcher, files) {
        Quickshell.execDetached(["gio", "launch", launcher].concat(files ?? []))
    }
    function openPicked() {
        const launchers = pickedPaths.filter(path => isLauncher(path))
        const files = pickedPaths.filter(path => !isLauncher(path))
        for (const launcher of launchers) {
            launch(launcher)
        }
        if (files.length > 0) {
            run(wishes.open_command, files)
        }
    }
    function trashPicked() {
        if (pickedPaths.length > 0) {
            run(wishes.trash_command, pickedPaths)
        }
    }
    function duplicatePicked() {
        if (pickedPaths.length > 0) {
            run(wishes.duplicate_command, pickedPaths)
        }
    }

    property string renamingPath: ""
    function startRenaming() {
        if (pickedPaths.length === 1 && (wishes.rename_command ?? []).length > 0) {
            renamingPath = pickedPaths[0]
        }
    }
    function finishRenaming(newName) {
        const path = renamingPath
        renamingPath = ""
        const oldName = path.substring(path.lastIndexOf("/") + 1)
        if (newName !== "" && newName !== oldName) {
            if (putByHand[oldName] !== undefined) {
                const places = Object.assign({}, putByHand)
                places[newName] = places[oldName]
                delete places[oldName]
                userChoices.set("desktop_files", "places", places)
            }
            run(wishes.rename_command, [path, newName])
        }
    }

    function newFolder() {
        run(wishes.new_folder_command, [folder, words.new_folder_name ?? "New Folder"])
    }
    function takeIn(paths) {
        if (paths.length > 0) {
            run(wishes.move_in_command, [folder].concat(paths))
        }
    }

    // --- The bare desktop: the band, the desktop's menu, dropped files ---------------------

    // The band: where the press was, where the pointer is, and what was picked
    // before it began (kept with Control or Shift).
    property bool banding: false
    property point bandFrom: Qt.point(0, 0)
    property point bandTo: Qt.point(0, 0)
    property var pickedBeforeBand: []
    readonly property rect band: Qt.rect(Math.min(bandFrom.x, bandTo.x), Math.min(bandFrom.y, bandTo.y),
                                         Math.abs(bandTo.x - bandFrom.x), Math.abs(bandTo.y - bandFrom.y))

    function pickInBand() {
        const reach = looks?.picked_reach ?? 4
        const touched = []
        for (let index = 0; index < entries.length; index++) {
            const left = iconX(index) - reach
            const top = iconY(index) - reach
            const side = iconSize + 2 * reach
            const apart = left > band.x + band.width || left + side < band.x
                || top > band.y + band.height || top + side < band.y
            if (!apart) {
                touched.push(entries[index].path)
            }
        }
        pickedPaths = pickedBeforeBand.filter(path => !touched.includes(path)).concat(touched)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPressed: mouse => {
            icons.forceActiveFocus()
            if (mouse.button === Qt.RightButton) {
                icons.pickNone()
                icons.openMenu("desktop", mouse.x, mouse.y)
                return
            }
            const adding = (mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0
            icons.pickedBeforeBand = adding ? icons.pickedPaths : []
            if (!adding) {
                icons.pickNone()
            }
            icons.bandFrom = Qt.point(mouse.x, mouse.y)
            icons.bandTo = icons.bandFrom
            icons.banding = true
        }
        onPositionChanged: mouse => {
            if (icons.banding) {
                icons.bandTo = Qt.point(mouse.x, mouse.y)
                icons.pickInBand()
            }
        }
        onReleased: icons.banding = false
    }

    function droppedPaths(drop) {
        const lines = text => text.split(/\r?\n/).filter(line => line !== "" && !line.startsWith("#"))
        let addresses = lines(drop.getDataAsString("text/uri-list"))
        // Where no desktop service runs, the address list comes back empty
        // and the plain text has the paths (as in Dock.qml, droppedAddresses).
        if (addresses.length === 0) {
            addresses = lines(drop.getDataAsString("text/plain"))
        }
        const paths = []
        for (const address of addresses) {
            if (address.startsWith("file://")) {
                paths.push(decodeURIComponent(address.substring("file://".length)))
            } else if (address.startsWith("/")) {
                paths.push(address)
            }
        }
        return paths
    }

    property string carriedPath: ""

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            const paths = icons.droppedPaths(drop)
            if (paths.length === 0) {
                return
            }
            // The desktop's own icon dropped here is placed; other files are moved in.
            if (icons.carriedPath !== "" && paths.includes(icons.carriedPath)) {
                icons.movePicked(icons.carriedPath, drop.x, drop.y)
            } else {
                icons.takeIn(paths)
            }
            drop.accept(Qt.MoveAction)
        }
    }

    focus: true
    Keys.onDeletePressed: trashPicked()
    Keys.onEscapePressed: pickNone()
    Keys.onSpacePressed: quickLook()
    Keys.onReturnPressed: startRenaming()

    // --- The icons -------------------------------------------------------------------------

    Repeater {
        model: icons.entries

        Item {
            id: fileItem
            required property var modelData
            required property int index
            readonly property bool picked: icons.isPicked(modelData.path)
            readonly property var name: icons.looks?.name ?? ({})

            x: icons.iconX(index)
            y: icons.iconY(index)
            width: icons.iconSize
            height: icons.iconSize

            Fill {
                readonly property real reach: icons.looks?.picked_reach ?? 4
                visible: fileItem.picked
                anchors.fill: parent
                anchors.margins: -reach
                fill: icons.looks?.picked ?? ({ color: "#00000040", radius: 4 })
                themeDirectory: icons.themeDirectory
            }
            // A picture is shown as itself, small, in the theme's frame; everything
            // else by the icon for its kind.
            readonly property bool isPicture: modelData.icon.startsWith("image-")
            readonly property string iconSource: themeIcons.path(modelData.icon, modelData.genericIcon)
            FramedPicture {
                id: preview
                anchors.fill: parent
                visible: fileItem.isPicture && !failed
                source: fileItem.isPicture ? "file://" + encodeURI(fileItem.modelData.path) : ""
                frame: icons.looks?.preview_frame ?? null
            }
            Image {
                id: picture
                anchors.fill: parent
                // Also for a picture in a format that cannot be read.
                visible: !preview.visible
                sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                source: fileItem.iconSource
            }

            Item {
                id: namePlate
                width: Math.min(label.implicitWidth, fileItem.name.most_width ?? 110) + 2 * (fileItem.name.padding ?? 6)
                height: fileItem.name.height ?? 18
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height + (fileItem.name.gap ?? 4)

                Fill {
                    visible: fileItem.picked
                    anchors.fill: parent
                    fill: fileItem.name.picked_background ?? ({ color: "#3a6ea5", radius: 4 })
                    themeDirectory: icons.themeDirectory
                }
                Text {
                    id: label
                    visible: icons.renamingPath !== fileItem.modelData.path
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, fileItem.name.most_width ?? 110)
                    elide: Text.ElideMiddle
                    text: fileItem.modelData.name
                    color: ThemeValues.color(fileItem.name.color ?? "#ffffff")
                    // A dark edge under the letters, readable on any picture.
                    style: fileItem.name.shadow_color !== undefined ? Text.Raised : Text.Normal
                    styleColor: ThemeValues.color(fileItem.name.shadow_color ?? "#00000000")
                    readonly property var face: icons.fonts.desktop_icon ?? icons.fonts.interface
                    font.family: face.family
                    font.pixelSize: face.size
                    font.bold: face.bold ?? false
                }
            }

            // Drag data: the picked files' addresses, and their paths as plain text for
            // programs that take only text. Qt needs an item to move while dragging.
            Item {
                id: carried
                Drag.active: press.drag.active
                Drag.onActiveChanged: {
                    if (Drag.active) {
                        icons.carriedPath = fileItem.modelData.path
                    }
                }
                Drag.dragType: Drag.Automatic
                Drag.supportedActions: Qt.CopyAction | Qt.MoveAction | Qt.LinkAction
                Drag.mimeData: ({
                    "text/uri-list": icons.pickedPaths.map(path => "file://" + encodeURI(path)).join("\r\n") + "\r\n",
                    "text/plain": icons.pickedPaths.join("\n")
                })
                // What follows the pointer: a snapshot of the icon taken on press, else the
                // icon for the file's kind.
                Drag.imageSource: fileItem.dragPicture !== "" ? fileItem.dragPicture : fileItem.iconSource
                Drag.imageSourceSize: Qt.size(icons.iconSize, icons.iconSize)
                Drag.hotSpot: fileItem.dragPicture !== "" ? fileItem.dragHold
                                                          : Qt.point(icons.iconSize / 2, icons.iconSize / 2)
                Drag.onDragFinished: {
                    x = 0
                    y = 0
                    icons.carriedPath = ""
                }
            }

            property string dragPicture: ""
            property point dragHold: Qt.point(0, 0)
            function takeDragPicture(holdX, holdY) {
                dragHold = Qt.point(Math.max(0, Math.min(width, holdX)), Math.max(0, Math.min(height, holdY)))
                fileItem.grabToImage(result => { fileItem.dragPicture = result.url })
            }

            // Files dropped on a program launcher are given to its program.
            DropArea {
                id: launcherDrop
                enabled: fileItem.modelData.isLauncher
                anchors.fill: parent
                onDropped: drop => {
                    const paths = icons.droppedPaths(drop)
                        .filter(path => path !== fileItem.modelData.path)
                    if (paths.length > 0) {
                        icons.launch(fileItem.modelData.path, paths)
                        drop.accept(Qt.CopyAction)
                    }
                }
            }
            Fill {
                readonly property real reach: icons.looks?.picked_reach ?? 4
                visible: launcherDrop.containsDrag
                anchors.fill: parent
                anchors.margins: -reach
                fill: icons.looks?.picked ?? ({ color: "#00000040", radius: 4 })
                themeDirectory: icons.themeDirectory
            }

            MouseArea {
                id: press
                x: Math.min(0, namePlate.x)
                width: Math.max(parent.width, namePlate.width)
                height: namePlate.y + namePlate.height
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                drag.target: carried

                onPressed: mouse => {
                    icons.forceActiveFocus()
                    const path = fileItem.modelData.path
                    if (mouse.button === Qt.LeftButton) {
                        const held = mapToItem(fileItem, mouse.x, mouse.y)
                        fileItem.takeDragPicture(held.x, held.y)
                    }
                    if (mouse.button === Qt.RightButton) {
                        // A file that is not picked becomes the only one picked.
                        if (!fileItem.picked) {
                            icons.pickOnly(path)
                        }
                        const place = mapToItem(icons, mouse.x, mouse.y)
                        icons.openFileMenu(path, fileItem.modelData.isFolder, place.x, place.y)
                    } else if ((mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0) {
                        icons.togglePicked(path)
                    } else if (!fileItem.picked) {
                        icons.pickOnly(path)
                    }
                }
                // A press on one of several picked files keeps them all picked so they can
                // be dragged together; released without a drag, it picks that file alone.
                onClicked: mouse => {
                    if (mouse.button === Qt.LeftButton
                        && (mouse.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) === 0) {
                        icons.pickOnly(fileItem.modelData.path)
                    }
                }
                onDoubleClicked: mouse => {
                    if (mouse.button === Qt.LeftButton) {
                        icons.openPicked()
                    }
                }
            }
        }
    }

    Rectangle {
        visible: icons.banding && (icons.band.width > 0 || icons.band.height > 0)
        x: icons.band.x
        y: icons.band.y
        width: icons.band.width
        height: icons.band.height
        color: ThemeValues.color(icons.looks?.band?.color ?? "#ffffff30")
        border.width: icons.looks?.band?.border_width ?? 1
        border.color: ThemeValues.color(icons.looks?.band?.border_color ?? "#ffffff90")
    }

    // --- Typing a new name ----------------------------------------------------------------
    //
    // The field for a new name: Return takes it, Escape or a press elsewhere
    // keeps the old one. It is a window of its own above the desktop because
    // the compositor gives the keyboard on request only to the layers in front,
    // and the background layer gets it only from a press.

    readonly property int renamingIndex: entries.findIndex(entry => entry.path === renamingPath)

    PanelWindow {
        screen: icons.screen
        visible: icons.renamingPath !== "" && icons.renamingIndex >= 0
        anchors {
            left: true
            right: true
            top: true
            bottom: true
        }
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.layer: WlrLayer.Overlay
        WlrLayershell.keyboardFocus: visible ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
        WlrLayershell.namespace: "desktop-rename"
        color: "transparent"

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: icons.finishRenaming("")
        }

        Loader {
            active: parent.visible
            sourceComponent: Rectangle {
                readonly property var field: icons.looks?.name_field ?? ({})
                readonly property var name: icons.looks?.name ?? ({})
                readonly property var face: icons.fonts.desktop_icon ?? icons.fonts.interface
                x: icons.iconX(icons.renamingIndex) + icons.iconSize / 2 - width / 2
                y: icons.iconY(icons.renamingIndex) + icons.iconSize + (name.gap ?? 4)
                width: Math.max(input.implicitWidth + 10, 60)
                height: name.height ?? 18
                color: ThemeValues.color(field.color ?? "#ffffff")
                border.width: 1
                border.color: ThemeValues.color(field.border_color ?? "#808080")

                TextInput {
                    id: input
                    anchors.centerIn: parent
                    focus: true
                    text: icons.entries[icons.renamingIndex]?.name ?? ""
                    color: ThemeValues.color(parent.field.text_color ?? "#000000")
                    selectionColor: ThemeValues.color(parent.field.selection_color ?? "#b4d5fe")
                    selectedTextColor: color
                    font.family: parent.face.family
                    font.pixelSize: parent.face.size
                    onAccepted: icons.finishRenaming(text)
                    Keys.onEscapePressed: icons.finishRenaming("")
                    // The name without its ending is selected, ready to be typed over; again
                    // when the keyboard arrives, since its arrival clears the selection.
                    function markStem() {
                        const dot = text.lastIndexOf(".")
                        select(0, dot > 0 ? dot : text.length)
                    }
                    Component.onCompleted: {
                        forceActiveFocus()
                        markStem()
                    }
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            markStem()
                        }
                    }
                }
            }
        }
    }

    // --- The menus ---------------------------------------------------------------------------

    property string menuKind: ""
    property point menuAt: Qt.point(0, 0)

    function openMenu(kind, x, y) {
        menuAt = Qt.point(x, y)
        menuKind = kind
    }

    function openFileMenu(path, isFolder, x, y) {
        menuAt = Qt.point(x, y)
        menuKind = "file"
    }

    // The orders the desktop's menu offers, as the folder listing names them.
    readonly property var orders: ["name", "kind", "modified", "added"]
    readonly property int idNewFolder: 1
    readonly property int idBackground: 2
    readonly property int idOpen: 3
    readonly property int idTrash: 4
    readonly property int idCleanUp: 5
    readonly property int idDuplicate: 6
    readonly property int idRename: 7
    readonly property int orderBase: 100

    readonly property var menuItems: {
        // Every field the menu's box reads has to be there.
        const separator = { id: 0, label: "", separator: true, enabled: false,
                            checkable: false, checked: false, shortcut: [], children: [] }
        const entry = (id, label, checked) =>
            ({ id: id, label: label, separator: false, enabled: true,
               checkable: checked !== undefined, checked: checked ?? false, shortcut: [], children: [] })
        if (menuKind === "file") {
            const items = [entry(idOpen, words.open ?? "Open")]
            items.push(separator)
            if (pickedPaths.length === 1 && (wishes.rename_command ?? []).length > 0) {
                items.push(entry(idRename, words.rename ?? "Rename"))
            }
            items.push(entry(idTrash, words.trash ?? "Move to Trash"))
            if ((wishes.duplicate_command ?? []).length > 0) {
                items.push(separator)
                items.push(entry(idDuplicate, words.duplicate ?? "Duplicate"))
            }
            return items
        }
        const items = [entry(idNewFolder, words.new_folder ?? "New Folder")]
        if ((wishes.background_command ?? []).length > 0) {
            items.push(separator)
            items.push(entry(idBackground, words.change_background ?? "Change Background…"))
        }
        items.push(separator)
        if (words.clean_up !== undefined) {
            items.push(entry(idCleanUp, words.clean_up))
        }
        for (let number = 0; number < orders.length; number++) {
            const word = words["sort_by_" + orders[number]]
            if (word !== undefined) {
                items.push(entry(orderBase + number, word, icons.order === orders[number]))
            }
        }
        return items
    }

    function menuChosen(id) {
        menuKind = ""
        if (id === idNewFolder) {
            newFolder()
        } else if (id === idBackground) {
            run(wishes.background_command, [])
        } else if (id === idOpen) {
            openPicked()
        } else if (id === idTrash) {
            trashPicked()
        } else if (id === idCleanUp) {
            cleanUp()
        } else if (id === idDuplicate) {
            duplicatePicked()
        } else if (id === idRename) {
            startRenaming()
        } else if (id >= orderBase) {
            userChoices.set("desktop_files", "places", ({}))
            userChoices.set("desktop_files", "order", orders[id - orderBase])
        }
    }

    MenuOverlay {
        screen: icons.screen
        settings: icons.settings
        visible: icons.menuKind !== ""
        items: icons.menuItems
        menuX: Math.max(0, Math.min(icons.menuAt.x, icons.width - menuWidth))
        menuY: Math.max(0, Math.min(icons.menuAt.y, icons.height - menuHeight))
        onChosen: id => icons.menuChosen(id)
        onDismissed: icons.menuKind = ""
    }
}
