// The window overview: the desktop shrinks onto a stage over a backdrop and
// every window is laid out in view, grouped by program with its icon and
// name. A press on a window activates it; Escape, the key again or a press
// beside the windows closes. `toggle()` is the action "window_overview".
// Theme: [window_overview], [animation.window_overview]; with `drawn_by`
// a theme leaves the overview to the compositor.
//
// The windows are still pictures the compositor takes when the overview
// opens (the shell cannot see other programs' windows); each moves from
// its place on screen to its place in the overview (WindowOverviewLayout.js).
// A strip along the top shows every desktop, to switch to, add or remove;
// the count is [desktops] count among the user's choices.

import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import Shell.Native
import "ThemeValues.js" as ThemeValues
import "WindowOverviewLayout.js" as Layout

PanelWindow {
    id: overview

    IconSource {
        id: themeIcons
        choices: overview.userChoices
    }

    required property var settings
    // What the desktop shows behind its windows.
    property var desktopFill: settings.theme.desktop.background
    // For reading and changing the number of desktops.
    required property var userChoices

    readonly property var theme: settings.theme
    readonly property var looks: theme.window_overview ?? ({})
    readonly property var motion: (theme.animation ?? ({})).window_overview ?? ({})
    readonly property var words: settings.config.window_overview ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    // --- Opening and closing ----------------------------------------------------

    // Open, or on its way there; `shown` follows from 0 to 1 and back.
    property bool open: false
    property real shown: open ? 1 : 0
    Behavior on shown {
        NumberAnimation {
            duration: overview.motion.duration_ms ?? 250
            easing.type: ThemeValues.easing(overview.motion.curve ?? "ease-in-out")
        }
    }
    // While true, the dock is kept in front of it.
    readonly property bool showing: open || shown > 0

    function toggle() {
        if (open) {
            open = false
        } else {
            // Opens once the pictures have arrived (onOverviewTaken).
            if (pictures.takeOverview === undefined) {
                console.warn("window overview: the shell's native module is older than this file;",
                             "rebuild it and start the desktop again")
                return
            }
            // The settings window may have changed the count, unannounced.
            desktops.refresh()
            pictures.takeOverview(pictureSide)
        }
    }

    // All windows as the compositor listed them, most recently used last (on
    // top). Positions are relative to the shown desktop's corner, so a window
    // on the next desktop is a screen's width further right.
    property var allWindows: []

    // The desktop shown, the count and the screen width as they were when the
    // windows were listed, from the same answer so they cannot disagree.
    property int currentDesktop: 0
    property int desktopCount: 1
    property real screenWidth: 0

    function desktopOf(window) {
        return window.desktop ?? currentDesktop
    }

    function windowsOn(desktop) {
        return allWindows.filter(window => desktopOf(window) === desktop)
    }

    readonly property var windows: windowsOn(overview.currentDesktop)
    readonly property var placement: Layout.place(windows, windowsArea,
        looks.most_window_scale ?? 0.85, looks.group_gap ?? 50)

    // Pictures are taken at this screen's size (the window has no size yet
    // when they are requested).
    readonly property int pictureSide: Math.round(
        Math.max(screen?.width ?? 0, screen?.height ?? 0)
        * (looks.most_window_scale ?? 0.85) * (screen?.devicePixelRatio ?? 1))

    // Falls back to the generic program icon.
    function iconFor(entry) {
        return themeIcons.forEntry(entry, "application-x-executable")
    }

    WindowPictures {
        id: pictures
        folder: Quickshell.env("XDG_RUNTIME_DIR") + "/mavernix/window-pictures"
    }
    Connections {
        target: pictures
        function onOverviewTaken(taken, shown) {
            overview.currentDesktop = shown?.current ?? desktops.current
            overview.desktopCount = shown?.count ?? desktops.count
            overview.screenWidth = shown?.screenWidth ?? desktops.screenWidth
            overview.allWindows = taken
            overview.hoveredId = -1
            overview.open = true
        }
    }

    // The compositor may switch desktops while the overview is open (swipe or
    // key); follow it, or the old desktop's windows stay on the stage.
    Connections {
        target: desktops
        function onChanged() {
            if (overview.open && desktops.current !== overview.currentDesktop) {
                overview.followDesktop(desktops.current)
            }
        }
    }

    // Switches the overview's desktop without asking for the windows again,
    // by shifting the origin all positions are relative to.
    function followDesktop(newDesktop) {
        const desktopsPassed = newDesktop - currentDesktop
        const shift_px = desktopsPassed * screenWidth
        allWindows = allWindows.map(window => Object.assign({}, window, {x: window.x - shift_px}))
        currentDesktop = newDesktop
        hoveredId = -1
    }

    // --- The desktops -----------------------------------------------------------

    Desktops {
        id: desktops
        // Its windows have been moved away, so the last one is now empty.
        onGapClosed: overview.setDesktopCount(overview.desktopCount - 1)
    }

    readonly property int mostDesktops: userChoices.value("desktops", "most") ?? 16

    // The compositor follows the setting shortly and unannounced, so the
    // pictures are taken again after a delay.
    function setDesktopCount(count) {
        userChoices.set("desktops", "count", count)
        afterCountChange.restart()
    }
    Timer {
        id: afterCountChange
        // Long enough for the session to rewrite the compositor's settings and the
        // compositor to reload them. Also used after moving a window.
        interval: 600
        onTriggered: {
            desktops.refresh()
            if (overview.open) {
                pictures.takeOverview(overview.pictureSide)
            }
        }
    }

    function addDesktop() {
        if (overview.desktopCount < mostDesktops) {
            setDesktopCount(overview.desktopCount + 1)
        }
    }

    function removeDesktop(index) {
        if (overview.desktopCount > 1) {
            desktops.closeGap(index)
        }
    }

    function goToDesktop(index) {
        desktops.show(index)
        open = false
    }

    // --- Taking a window to another desktop -------------------------------------
    //
    // Dragging a window's picture onto a desktop in the strip moves it there.

    // -1 for none.
    property int draggedId: -1
    property point dragOffset: Qt.point(0, 0)
    property int dragTarget: -1

    function stripPictureX(index) {
        return (width - stripPictureWidth) / 2 + (index - (overview.desktopCount - 1) / 2) * stripPitch
    }

    // -1 for none.
    function desktopPictureAt(x, y) {
        const top = stripLooks.top ?? 22
        if (looks.strip === undefined || y < top || y > top + stripPictureHeight) {
            return -1
        }
        for (let index = 0; index < overview.desktopCount; index++) {
            const left = stripPictureX(index)
            if (x >= left && x <= left + stripPictureWidth) {
                return index
            }
        }
        return -1
    }

    // Keeps the window's position relative to its desktop.
    function sendToDesktop(window, desktop) {
        const desktopsAway = desktop - desktopOf(window)
        if (desktopsAway === 0) {
            return
        }
        desktops.placeWindow(window.id, window.x + desktopsAway * overview.screenWidth, window.y,
                             window.width, window.height)
        afterCountChange.restart()
    }

    function choose(id) {
        pictures.focusWindow(id)
        open = false
    }

    // --- The window -------------------------------------------------------------

    visible: showing
    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    exclusionMode: ExclusionMode.Ignore
    // Above windows but below what the shell raises (the dock); the panel
    // shares this layer and is concealed meanwhile.
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: open ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: overview.open = false
    }

    // --- Backdrop and stage -----------------------------------------------------

    // The shrunk desktop, placed by shares of the screen size.
    readonly property real stageScale: looks.stage_scale ?? 0.7
    readonly property rect stage: Qt.rect(
        (width - width * stageScale) / 2,
        height * (looks.stage_top_share ?? ((1 - stageScale) / 2)),
        width * stageScale, height * stageScale)

    // Where the windows may lie: between the desktop strip and the dock.
    readonly property rect windowsArea: {
        const side = looks.windows_side_margin ?? 30
        const top = height * (looks.windows_top_share ?? 0.2)
        const bottom = height * (looks.windows_bottom_share ?? 0.78)
        return Qt.rect(side, top, width - 2 * side, bottom - top)
    }

    // Interpolates between the on-screen value and the overview value.
    function between(onScreen, inOverview) {
        return onScreen + (inOverview - onScreen) * shown
    }

    MouseArea {
        anchors.fill: parent
        onClicked: overview.open = false
    }

    Fill {
        anchors.fill: parent
        fill: overview.looks.backdrop ?? ({ color: "#303034" })
        themeDirectory: overview.themeDirectory
        // Opaque from the start, hiding the real windows; the full-screen stage
        // covers it at first.
    }

    Item {
        id: stageItem
        x: overview.between(0, overview.stage.x)
        y: overview.between(0, overview.stage.y)
        width: overview.between(overview.width, overview.stage.width)
        height: overview.between(overview.height, overview.stage.height)

        RectangularShadow {
            visible: overview.looks.stage_shadow !== undefined
            anchors.fill: parent
            color: ThemeValues.color(overview.looks.stage_shadow?.color ?? "#00000000")
            blur: overview.looks.stage_shadow?.blur ?? 0
            opacity: overview.shown
        }
        // Drawn once at screen size and scaled as a whole: drawn at the stage's
        // size, it was reloaded at every animation step and stayed dark.
        Fill {
            id: desktopFill
            width: overview.width
            height: overview.height
            fill: overview.desktopFill
            themeDirectory: overview.themeDirectory
            transformOrigin: Item.TopLeft
            scale: overview.width > 0 ? stageItem.width / overview.width : 1
        }
    }

    // --- The strip of desktops --------------------------------------------------

    readonly property var stripLooks: looks.strip ?? ({})
    readonly property real stripPictureWidth: width * (stripLooks.picture_scale ?? 0.1)
    readonly property real stripPictureHeight: height * (stripLooks.picture_scale ?? 0.1)
    readonly property real stripPitch: stripPictureWidth + (stripLooks.picture_gap ?? 40)

    // `mipmap`: shrunk to a tenth, it would otherwise shimmer.
    component SmallDesktop: ShaderEffectSource {
        sourceItem: desktopFill
        mipmap: true
        smooth: true
    }

    Item {
        id: strip
        visible: overview.looks.strip !== undefined
        opacity: overview.shown
        width: parent.width
        height: (overview.stripLooks.top ?? 22) + overview.stripPictureHeight
        y: (overview.shown - 1) * (overview.stripLooks.slide ?? 30)

        Repeater {
            model: overview.desktopCount

            Item {
                id: desktopItem
                required property int index

                width: overview.stripPictureWidth
                height: overview.stripPictureHeight
                x: overview.stripPictureX(index)
                y: overview.stripLooks.top ?? 22

                SmallDesktop {
                    anchors.fill: parent
                }
                Item {
                    anchors.fill: parent
                    clip: true

                    Repeater {
                        model: overview.windowsOn(desktopItem.index)

                        Image {
                            required property var modelData
                            readonly property real scaleDown: overview.stripLooks.picture_scale ?? 0.1
                            // Positions are relative to the shown desktop; here, to its own.
                            x: (modelData.x - (desktopItem.index - overview.currentDesktop) * overview.screenWidth) * scaleDown
                            y: modelData.y * scaleDown
                            width: modelData.width * scaleDown
                            height: modelData.height * scaleDown
                            source: modelData.picture
                            cache: false
                            smooth: true
                            mipmap: true
                        }
                    }
                }
                Rectangle {
                    readonly property real lineWidth: overview.stripLooks.mark_width ?? 2
                    visible: desktopItem.index === overview.currentDesktop || desktopItem.index === overview.dragTarget
                    anchors.fill: parent
                    anchors.margins: -lineWidth
                    color: "transparent"
                    border.width: lineWidth
                    border.color: ThemeValues.color(overview.stripLooks.mark_color ?? "#ffffff")
                }
                OverviewLabel {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height + (overview.stripLooks.name_gap ?? 12)
                    text: overview.desktopCount === 1
                        ? (overview.words.desktop_name ?? "")
                        : (overview.words.desktop_numbered_name ?? "%1").replace("%1", desktopItem.index + 1)
                }

                MouseArea {
                    id: desktopArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: overview.goToDesktop(desktopItem.index)
                }

                // The remove button, on the picture's corner after the pointer rests a
                // while. The only desktop cannot be removed.
                readonly property var closeButton: overview.stripLooks.close_button ?? ({})
                property bool rested: false
                HoverHandler {
                    id: pointerOver
                    // The button overhangs the corner; the pointer there is still "on".
                    margin: (desktopItem.closeButton.size ?? 0) / 2
                    onHoveredChanged: {
                        desktopItem.rested = false
                        if (hovered) {
                            restingOnPicture.restart()
                        } else {
                            restingOnPicture.stop()
                        }
                    }
                }
                Timer {
                    id: restingOnPicture
                    interval: overview.stripLooks.close_delay_ms ?? 300
                    onTriggered: desktopItem.rested = true
                }
                // Without the theme's picture, a plain round button.
                CloseCircle {
                    visible: overview.stripLooks.close_button === undefined && overview.desktopCount > 1 && desktopItem.rested
                    x: -width / 2
                    y: -height / 2
                    onClicked: overview.removeDesktop(desktopItem.index)
                }
                Image {
                    visible: overview.stripLooks.close_button !== undefined && overview.desktopCount > 1 && desktopItem.rested
                    source: overview.stripLooks.close_button !== undefined
                        ? "file://" + overview.themeDirectory + "/"
                            + (closeArea.pressed ? (desktopItem.closeButton.pressed ?? desktopItem.closeButton.image)
                                                 : desktopItem.closeButton.image)
                        : ""
                    width: desktopItem.closeButton.size ?? 0
                    height: desktopItem.closeButton.size ?? 0
                    x: -width / 2
                    y: -height / 2

                    MouseArea {
                        id: closeArea
                        anchors.fill: parent
                        onClicked: overview.removeDesktop(desktopItem.index)
                    }
                }
            }
        }

        // The add tile: a desktop picture with a plus that slides halfway out of
        // the far top corner when the pointer comes near.
        Item {
            id: addTile
            readonly property var tile: overview.stripLooks.add_tile ?? ({})
            readonly property bool wanted: nearCorner.containsMouse && overview.desktopCount < overview.mostDesktops
            // 0 hidden, 1 fully out.
            property real out: wanted ? 1 : 0
            Behavior on out {
                NumberAnimation {
                    duration: addTile.tile.slide_ms ?? 150
                }
            }

            visible: out > 0
            width: overview.stripPictureWidth
            height: overview.stripPictureHeight
            x: strip.width - out * width * (tile.shown_share ?? 0.5)
            y: overview.stripLooks.top ?? 22

            SmallDesktop {
                anchors.fill: parent
                opacity: addTile.tile.opacity ?? 0.6
            }
            // Without the theme's picture, a plain plus.
            Text {
                visible: addTile.tile.image === undefined
                text: "+"
                color: "white"
                font.pixelSize: 28
                font.bold: true
                style: Text.Raised
                styleColor: "#80000000"
                x: (parent.width * (addTile.tile.shown_share ?? 0.5) - width) / 2
                anchors.verticalCenter: parent.verticalCenter
            }
            Image {
                source: addTile.tile.image !== undefined ? "file://" + overview.themeDirectory + "/" + addTile.tile.image : ""
                width: (addTile.tile.image_size ?? [24, 24])[0]
                height: (addTile.tile.image_size ?? [24, 24])[1]
                x: (parent.width * (addTile.tile.shown_share ?? 0.5) - width) / 2
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        MouseArea {
            id: nearCorner
            width: overview.stripPictureWidth
            height: strip.height + (overview.stripLooks.name_gap ?? 12)
            x: strip.width - width
            hoverEnabled: true
            onClicked: overview.addDesktop()
        }
    }

    // Bold light text, with an optional dark shadow one row lower.
    component OverviewLabel: Item {
        id: label
        property string text: ""
        readonly property var face: overview.theme.fonts.window_overview_label ?? overview.theme.fonts.interface
        width: front.implicitWidth
        height: front.implicitHeight

        Text {
            visible: overview.looks.label_shadow_color !== undefined
            y: 1
            text: label.text
            color: ThemeValues.color(overview.looks.label_shadow_color ?? "#00000000")
            font: front.font
        }
        Text {
            id: front
            text: label.text
            color: ThemeValues.color(overview.looks.label_color ?? "#ffffff")
            font.family: label.face.family
            font.pixelSize: label.face.size
            font.bold: label.face.bold ?? true
        }
    }

    // --- The windows ------------------------------------------------------------

    // -1 for none.
    property int hoveredId: -1

    Repeater {
        model: overview.windows

        Item {
            id: windowItem
            required property var modelData
            readonly property var place: overview.placement.places[modelData.id]
                ?? ({ x: modelData.x, y: modelData.y, width: modelData.width, height: modelData.height })

            readonly property bool dragged: overview.draggedId === modelData.id
            x: overview.between(modelData.x, place.x) + (dragged ? overview.dragOffset.x : 0)
            y: overview.between(modelData.y, place.y) + (dragged ? overview.dragOffset.y : 0)
            width: overview.between(modelData.width, place.width)
            height: overview.between(modelData.height, place.height)
            z: dragged ? 1 : 0

            Fill {
                readonly property real reach: overview.looks.selection_reach ?? 4
                visible: overview.hoveredId === windowItem.modelData.id && overview.open
                    && overview.looks.selection !== undefined
                anchors.fill: parent
                anchors.margins: -reach
                fill: overview.looks.selection ?? ({ color: "#00000000" })
                themeDirectory: overview.themeDirectory
            }
            Image {
                anchors.fill: parent
                source: windowItem.modelData.picture
                cache: false
                smooth: true
                mipmap: true
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: overview.hoveredId = windowItem.modelData.id
                onExited: {
                    if (overview.hoveredId === windowItem.modelData.id) {
                        overview.hoveredId = -1
                    }
                }
                // Movement beyond this makes a press a drag.
                readonly property real dragStart: 8
                // In screen coordinates, since the item itself moves with the drag.
                property point pressedAt: Qt.point(0, 0)
                property bool wasDragged: false

                onPressed: mouse => {
                    pressedAt = mapToItem(null, mouse.x, mouse.y)
                    wasDragged = false
                }
                onPositionChanged: mouse => {
                    if (!pressed) {
                        return
                    }
                    const now = mapToItem(null, mouse.x, mouse.y)
                    const moved = Qt.point(now.x - pressedAt.x, now.y - pressedAt.y)
                    if (!wasDragged && Math.hypot(moved.x, moved.y) < dragStart) {
                        return
                    }
                    wasDragged = true
                    overview.draggedId = windowItem.modelData.id
                    overview.dragOffset = moved
                    overview.dragTarget = overview.desktopPictureAt(now.x, now.y)
                }
                onReleased: {
                    if (wasDragged && overview.dragTarget !== -1) {
                        overview.sendToDesktop(windowItem.modelData, overview.dragTarget)
                    }
                    overview.draggedId = -1
                    overview.dragOffset = Qt.point(0, 0)
                    overview.dragTarget = -1
                }
                onClicked: {
                    if (!wasDragged) {
                        overview.choose(windowItem.modelData.id)
                    }
                }
            }
            // Under the pointer, a button on the corner closes the window
            // without going to it first.
            CloseCircle {
                visible: overview.hoveredId === windowItem.modelData.id && overview.open && !windowItem.dragged
                // Inside the picture, so that the pointer on it is still on the window.
                x: 6
                y: 6
                onClicked: {
                    desktops.closeWindow(windowItem.modelData.id)
                    overview.hoveredId = -1
                }
            }
        }
    }

    // A round close button drawn without the theme: dark, with a light cross.
    component CloseCircle: Rectangle {
        id: circle
        signal clicked()
        width: 24
        height: 24
        radius: width / 2
        color: circleArea.pressed ? "#303030" : "#505050"
        border.color: "#e0e0e0"
        border.width: 2
        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            text: "×"
            color: "white"
            font.pixelSize: 18
            font.bold: true
        }
        // No hover of its own, so the window under it keeps the pointer's hover.
        MouseArea {
            id: circleArea
            anchors.fill: parent
            onClicked: circle.clicked()
        }
    }

    // --- Each program's icon and name, under its windows ------------------------

    Repeater {
        model: overview.placement.groups

        Item {
            id: groupItem
            required property var modelData
            readonly property var entry: DesktopEntries.heuristicLookup(modelData.appId)
            readonly property real iconSize: overview.looks.group_icon_size ?? 32

            opacity: overview.shown
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height

            // An optional soft shadow picture, at its own size, centred a little below
            // the icon.
            Image {
                readonly property var shadow: overview.looks.group_icon_shadow ?? ({})
                visible: overview.looks.group_icon_shadow !== undefined
                source: visible ? "file://" + overview.themeDirectory + "/" + shadow.image : ""
                width: shadow.size ?? 0
                height: shadow.size ?? 0
                x: groupIcon.x + (groupIcon.width - width) / 2
                y: groupIcon.y + (groupIcon.height - height) / 2 + (shadow.drop ?? 0)
            }
            Image {
                id: groupIcon
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height - height / 2 - (overview.looks.group_icon_rise ?? 5)
                width: groupItem.iconSize
                height: groupItem.iconSize
                sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                source: overview.iconFor(groupItem.entry)
            }
            OverviewLabel {
                anchors.horizontalCenter: parent.horizontalCenter
                y: groupIcon.y + groupIcon.height + (overview.looks.group_name_gap ?? 4)
                text: groupItem.entry?.name ?? groupItem.modelData.appId
            }
        }
    }
}
