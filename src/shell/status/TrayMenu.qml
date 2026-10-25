// Reads another program's menu (StatusNotifierItem) into the panel's menu
// form, one level per instance; each submenu gets an instance of its own.
//
// An entry's id is its level's `base` plus its place plus one; the level
// below uses that id times `places` as its base. With 64 places per level,
// five levels stay within the integers QML handles exactly.

import QtQuick
import Quickshell

Item {
    id: level

    property var menu: null
    property int base: 0

    readonly property int places: 64

    QsMenuOpener {
        id: opener
        menu: level.menu
    }
    readonly property var entries: opener.children.values

    // One loader per entry, active where it has a submenu. (A component cannot
    // name itself as a child, so the copies are loaded by file name.)
    Repeater {
        id: below
        model: level.entries

        Loader {
            required property var modelData
            required property int index
            active: modelData.hasChildren
            source: "TrayMenu.qml"
            onLoaded: {
                item.menu = modelData
                item.base = (level.base + index + 1) * level.places
                item.itemsChanged.connect(level.lowerLevelChanged)
            }
        }
    }

    // Bumped when a submenu's entries change, so `items` is recomputed.
    property int changesBelow: 0
    function lowerLevelChanged() {
        changesBelow += 1
    }

    readonly property var items: {
        changesBelow
        const list = []
        for (let place = 0; place < entries.length; place++) {
            const entry = entries[place]
            const lower = below.itemAt(place)
            list.push({
                id: base + place + 1,
                label: entry.text,
                enabled: entry.enabled,
                separator: entry.isSeparator,
                checkable: entry.buttonType !== QsMenuButtonType.None,
                checked: entry.checkState === Qt.Checked,
                shortcut: [],
                children: (lower !== null && lower.item !== null) ? lower.item.items : []
            })
        }
        return list
    }

    // Triggers the entry with this id; returns whether it was found.
    function choose(id) {
        const place = id - base - 1
        if (place >= 0 && place < entries.length) {
            entries[place].triggered()
            return true
        }
        for (let index = 0; index < below.count; index++) {
            const lower = below.itemAt(index)
            if (lower !== null && lower.item !== null && lower.item.choose(id)) {
                return true
            }
        }
        return false
    }
}
