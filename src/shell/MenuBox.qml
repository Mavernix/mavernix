// One menu: a box of items, with submenus beside it. Theme: [menu] and
// [fonts].menu. Items are plain objects, as the native ApplicationMenu
// gives them:
//
//   { id, label, enabled, separator, checkable, checked, shortcut, children }
//
// An item may also have `pictures`: image addresses drawn small at its end
// in the text's colour, such as a lock and a signal strength.
//
// This is an item, not a window: MenuOverlay.qml holds open menus in one
// full-screen window. (Pop-up windows showed stale, stretched contents
// after a change, and pointer positions came out wrong.)

import QtQuick
import QtQuick.Effects
import QtQuick.Shapes
import "ThemeValues.js" as ThemeValues

Item {
    id: menu

    required property var settings
    property var items: []

    // Square corners where the menu hangs from something: the panel along the
    // whole top edge, a parent item at one corner.
    property bool squareTopLeft: false
    property bool squareTopRight: false

    signal chosen(int id)
    // Applications may want to fill a submenu before it is shown.
    signal submenuWanted(int id)
    // An item with a `slider` value (0 to 1) is drawn as a slider; it should
    // not be `enabled`, so the keyboard mark passes over it.
    signal slid(int id, real value)

    // The theme's [menu] table, or one of the same shape (the dock's).
    property var looks: settings.theme.menu

    // The pointer tip's x along the bottom edge, from the menu's left edge;
    // negative, or no `pointer` in the looks, for none.
    property real pointerX: -1
    readonly property var font: settings.theme.fonts.menu

    // -1 for none.
    property int hoveredIndex: -1
    property int submenuIndex: -1

    onItemsChanged: {
        hoveredIndex = -1
        submenuIndex = -1
    }

    // --- Moving through the menu by key -----------------------------------------
    //
    // MenuOverlay passes the keys on. The keyboard mark is the same one the
    // pointer moves (hoveredIndex).

    property var parentMenu: null
    property var openSubmenu: null

    // The innermost open menu, which keys act on.
    function innermost() {
        return openSubmenu !== null ? openSubmenu.innermost() : menu
    }

    function canBeMarked(index) {
        const item = items[index]
        return item !== undefined && !item.separator && item.enabled
    }

    // Moves the mark to the next enabled item (1 down, -1 up), wrapping.
    function moveMark(by) {
        const count = items.length
        let index = hoveredIndex
        for (let step = 0; step < count; step++) {
            // Without a mark, down starts at the top and up at the bottom.
            index = index < 0 ? (by > 0 ? 0 : count - 1) : (index + by + count) % count
            if (canBeMarked(index)) {
                hoveredIndex = index
                submenuIndex = -1
                return
            }
        }
    }

    // Marks the first enabled item whose name starts with `text`, ignoring case.
    function markByName(beginning) {
        const wanted = beginning.toLowerCase()
        for (let index = 0; index < items.length; index++) {
            if (canBeMarked(index) && items[index].label.toLowerCase().startsWith(wanted)) {
                hoveredIndex = index
                submenuIndex = -1
                return
            }
        }
    }

    // Returns whether there was a submenu to open.
    function openMarkedSubmenu() {
        if (!canBeMarked(hoveredIndex) || items[hoveredIndex].children.length === 0) {
            return false
        }
        submenuWanted(items[hoveredIndex].id)
        submenuIndex = hoveredIndex
        if (openSubmenu !== null) {
            openSubmenu.moveMark(1)
        }
        return true
    }

    // Returns whether this menu was a submenu.
    function closeAsSubmenu() {
        if (parentMenu === null) {
            return false
        }
        parentMenu.submenuIndex = -1
        return true
    }

    function chooseMarked() {
        if (!canBeMarked(hoveredIndex)) {
            return
        }
        if (!openMarkedSubmenu()) {
            chosen(items[hoveredIndex].id)
        }
    }

    // The widest row decides the menu's width.
    property real widestRow: 0
    function measureRows() {
        let widest = 0
        for (let index = 0; index < rowItems.count; index++) {
            const rowItem = rowItems.itemAt(index)
            if (rowItem !== null) {
                widest = Math.max(widest, rowItem.neededWidth)
            }
        }
        widestRow = widest
    }

    width: Math.ceil(widestRow)
    height: rows.height + 2 * looks.padding

    // Clicks on padding or separators must not fall through.
    MouseArea {
        anchors.fill: parent
    }

    // Each key by the theme's symbol for it if any, else by its name.
    function shortcutText(keys) {
        let text = ""
        for (const key of keys) {
            text += menu.looks.key_symbols[key] ?? key
        }
        return text
    }

    BoxFrame {
        looks: menu.looks
        squareTopLeft: menu.squareTopLeft
        squareTopRight: menu.squareTopRight
        pointerX: menu.pointerX
    }

    Column {
        id: rows
        y: menu.looks.padding
        width: parent.width

        Repeater {
            id: rowItems
            model: menu.items
            onItemAdded: menu.measureRows()
            onItemRemoved: menu.measureRows()

            Item {
                id: row
                required property int index
                required property var modelData

                readonly property bool isSeparator: modelData.separator
                readonly property bool hasSubmenu: modelData.children.length > 0
                readonly property bool selected:
                    !isSeparator && modelData.enabled
                    && (menu.hoveredIndex === index || menu.submenuIndex === index)

                // The text inset, the label, and a shortcut or submenu arrow after a gap.
                readonly property real endWidth:
                    Math.max(shortcut.implicitWidth, arrow.implicitWidth, endPictures.implicitWidth)
                readonly property bool isSlider: modelData.slider !== undefined
                readonly property real neededWidth:
                    isSlider ? (menu.looks.slider_min_width ?? 180) :
                    menu.looks.text_inset + label.implicitWidth
                    + (endWidth > 0 ? menu.looks.shortcut_gap + endWidth : 0)
                    + menu.looks.shortcut_end_inset
                onNeededWidthChanged: menu.measureRows()

                width: rows.width
                height: isSeparator ? menu.looks.separator_height
                                    : (isSlider ? (menu.looks.slider_row_height ?? menu.looks.item_height + 4)
                                                : menu.looks.item_height)

                readonly property color textColor: ThemeValues.color(
                    selected ? menu.looks.highlight_text_color
                             : (modelData.enabled ? menu.looks.text_color : menu.looks.disabled_text_color))

                // On the row the theme names (counted from 1).
                Rectangle {
                    visible: row.isSeparator
                    y: menu.looks.separator_line_row - 1
                    width: parent.width
                    height: 1
                    color: ThemeValues.color(menu.looks.separator_color)
                }

                Fill {
                    anchors.fill: parent
                    visible: row.selected
                    fill: menu.looks.highlight
                    themeDirectory: menu.settings.theme_directory
                }

                component RowText: Text {
                    visible: !row.isSeparator
                    color: row.textColor
                    font.family: menu.font.family
                    font.pixelSize: menu.font.size
                    // The theme's baseline row (counted from 1).
                    y: menu.looks.text_baseline_row - baselineOffset
                }

                // A tick drawn from check_mark_shape, else the theme's character.
                readonly property var tickShape: menu.looks.check_mark_shape
                RowText {
                    visible: !row.isSeparator && row.modelData.checked && row.tickShape === undefined
                    text: menu.looks.check_mark
                    x: (menu.looks.text_inset - implicitWidth) / 2
                }
                Shape {
                    visible: !row.isSeparator && row.modelData.checked && row.tickShape !== undefined
                    width: row.tickShape?.width ?? 0
                    height: row.tickShape?.height ?? 0
                    x: row.tickShape?.left_inset ?? 0
                    y: Math.round(label.y + label.baselineOffset - (row.tickShape?.middle_above_baseline ?? 4) - height / 2)
                    layer.enabled: true
                    layer.samples: 4
                    ShapePath {
                        id: tickStrokes
                        readonly property var from: row.tickShape?.start ?? [0, 0]
                        readonly property var bend: row.tickShape?.bend ?? [0, 0]
                        readonly property var to: row.tickShape?.end ?? [0, 0]
                        fillColor: "transparent"
                        strokeWidth: row.tickShape?.stroke ?? 2
                        strokeColor: row.selected || row.tickShape?.color === undefined
                            ? row.textColor : ThemeValues.color(row.tickShape.color)
                        capStyle: ShapePath.FlatCap
                        joinStyle: ShapePath.MiterJoin
                        startX: from[0]
                        startY: from[1]
                        PathLine { x: tickStrokes.bend[0]; y: tickStrokes.bend[1] }
                        PathLine { x: tickStrokes.to[0]; y: tickStrokes.to[1] }
                    }
                }

                RowText {
                    id: label
                    x: menu.looks.text_inset
                    text: row.modelData.label
                }

                RowText {
                    id: shortcut
                    text: menu.shortcutText(row.modelData.shortcut)
                    x: parent.width - menu.looks.shortcut_end_inset - implicitWidth
                }

                Row {
                    id: endPictures
                    visible: !row.isSeparator
                    spacing: 4
                    readonly property int side: Math.round(menu.font.size)
                    x: parent.width - menu.looks.shortcut_end_inset - implicitWidth
                    y: Math.round(label.y + label.baselineOffset - side * 0.4 - side / 2)
                    Repeater {
                        model: row.modelData.pictures ?? []
                        Image {
                            required property string modelData
                            source: modelData
                            width: endPictures.side
                            height: endPictures.side
                            sourceSize.width: width * Screen.devicePixelRatio
                            sourceSize.height: height * Screen.devicePixelRatio
                            fillMode: Image.PreserveAspectFit
                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1
                                colorizationColor: row.textColor
                            }
                        }
                    }
                }

                // A triangle from submenu_arrow_shape, else the theme's character.
                readonly property var arrowShape: menu.looks.submenu_arrow_shape
                RowText {
                    id: arrow
                    text: row.hasSubmenu && row.arrowShape === undefined ? menu.looks.submenu_arrow : ""
                    x: parent.width - menu.looks.shortcut_end_inset - implicitWidth
                }
                Shape {
                    visible: row.hasSubmenu && row.arrowShape !== undefined
                    width: row.arrowShape?.width ?? 0
                    height: row.arrowShape?.height ?? 0
                    x: parent.width - (row.arrowShape?.end_inset ?? menu.looks.shortcut_end_inset) - width
                    y: Math.round(label.y + label.baselineOffset - (row.arrowShape?.middle_above_baseline ?? 5) - height / 2)
                    layer.enabled: true
                    layer.samples: 4
                    ShapePath {
                        strokeWidth: -1
                        // The text colour, unless the theme gives one for the unmarked state.
                        fillColor: row.selected || row.arrowShape?.color === undefined
                            ? row.textColor : ThemeValues.color(row.arrowShape.color)
                        startX: 0
                        startY: 0
                        PathLine { x: row.arrowShape?.width ?? 0; y: (row.arrowShape?.height ?? 0) / 2 }
                        PathLine { x: 0; y: row.arrowShape?.height ?? 0 }
                        PathLine { x: 0; y: 0 }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    // Follows pointer movement, not presence, so a menu opening under a
                    // resting pointer does not light an item.
                    onPositionChanged: {
                        if (menu.hoveredIndex === row.index) {
                            return
                        }
                        menu.hoveredIndex = row.index
                        if (row.hasSubmenu && row.modelData.enabled) {
                            menu.submenuWanted(row.modelData.id)
                            menu.submenuIndex = row.index
                        } else {
                            menu.submenuIndex = -1
                        }
                    }
                    onExited: {
                        if (menu.hoveredIndex === row.index) {
                            menu.hoveredIndex = -1
                        }
                    }
                    onClicked: {
                        if (!row.isSeparator && row.modelData.enabled && !row.hasSubmenu) {
                            menu.chosen(row.modelData.id)
                        }
                    }
                }

                // A groove and knob from [controls.slider] if the theme has them, else
                // drawn plainly. A press or drag anywhere sets the value.
                Item {
                    id: sliderItem
                    visible: row.isSlider
                    readonly property var pictures: menu.settings.theme.controls?.slider ?? null
                    readonly property string themeDirectory: menu.settings.theme_directory
                    readonly property real inset: menu.looks.slider_inset ?? menu.looks.text_inset
                    // Set from the item when the row is made, then by dragging. (Changing the
                    // items while dragging would make new rows and lose the drag.)
                    property real value: Math.max(0, Math.min(1, row.modelData.slider ?? 0))
                    x: inset
                    width: parent.width - 2 * inset
                    height: parent.height
                    // The knob's centre travels between points half a knob in from each end.
                    readonly property real knobSide: pictures !== null ? knob.implicitWidth : (menu.looks.slider_knob_size ?? 15)
                    readonly property real travel: width - knobSide

                    BorderImage {
                        visible: sliderItem.pictures !== null
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        source: sliderItem.pictures !== null ? "file://" + sliderItem.themeDirectory + "/" + sliderItem.pictures.track : ""
                        readonly property var slice: sliderItem.pictures?.track_slice ?? [0, 0, 0, 0]
                        border.top: slice[0]
                        border.right: slice[1]
                        border.bottom: slice[2]
                        border.left: slice[3]
                    }
                    Rectangle {
                        visible: sliderItem.pictures === null
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 4
                        radius: 2
                        color: ThemeValues.color(menu.looks.separator_color)
                    }
                    Image {
                        id: knob
                        visible: sliderItem.pictures !== null
                        anchors.verticalCenter: parent.verticalCenter
                        x: sliderItem.value * sliderItem.travel
                        source: sliderItem.pictures !== null
                            ? "file://" + sliderItem.themeDirectory + "/" + (slideArea.pressed ? (sliderItem.pictures.thumb_pressed ?? sliderItem.pictures.thumb) : sliderItem.pictures.thumb)
                            : ""
                        width: sliderItem.pictures !== null ? implicitWidth : sliderItem.knobSide
                    }
                    Rectangle {
                        visible: sliderItem.pictures === null
                        anchors.verticalCenter: parent.verticalCenter
                        x: sliderItem.value * sliderItem.travel
                        width: sliderItem.knobSide
                        height: sliderItem.knobSide
                        radius: sliderItem.knobSide / 2
                        color: ThemeValues.color(slideArea.pressed ? menu.looks.highlight_text_color : menu.looks.text_color)
                    }
                    MouseArea {
                        id: slideArea
                        anchors.fill: parent
                        function set(x) {
                            const share = (x - sliderItem.knobSide / 2) / Math.max(1, sliderItem.travel)
                            sliderItem.value = Math.max(0, Math.min(1, share))
                            menu.slid(row.modelData.id, sliderItem.value)
                        }
                        onPressed: mouse => set(mouse.x)
                        onPositionChanged: mouse => {
                            if (pressed) {
                                set(mouse.x)
                            }
                        }
                    }
                }

                // A component cannot contain itself, so the file is loaded by name.
                Loader {
                    id: submenu
                    readonly property bool wanted: menu.submenuIndex === row.index
                    onWantedChanged: {
                        if (!wanted) {
                            menu.openSubmenu = null
                            source = ""
                            return
                        }
                        setSource("MenuBox.qml", {
                            settings: menu.settings,
                            looks: menu.looks,
                            parentMenu: menu,
                            items: row.modelData.children
                        })
                    }
                    // In window coordinates (`null`).
                    readonly property bool fitsAfter: item === null
                        || row.mapToItem(null, row.width, 0).x + item.width <= menu.Window.width
                    onLoaded: {
                        menu.openSubmenu = item
                        // After this item, or before it if it would not fit; raised by the padding
                        // so its first item is level with this one. Bindings, because the submenu
                        // only knows its width once its rows are measured.
                        item.x = Qt.binding(() => submenu.fitsAfter ? row.width : -item.width)
                        item.y = -menu.looks.padding
                        item.squareTopLeft = Qt.binding(() => submenu.fitsAfter)
                        item.squareTopRight = Qt.binding(() => !submenu.fitsAfter)
                        item.chosen.connect(menu.chosen)
                        item.submenuWanted.connect(menu.submenuWanted)
                        item.slid.connect(menu.slid)
                    }
                }
            }
        }
    }
}
