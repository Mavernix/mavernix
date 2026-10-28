// One item of the panel's status area: a one-colour icon showing a state,
// optionally with text before it and a menu under it. Status item files
// (docs/panel-items.md) are built on it.
//
// Icons use freedesktop names without "-symbolic". A theme may supply its
// own picture per name (panel.status.icons); otherwise the icon theme's
// symbolic icon is used, so an item works under any theme.

import QtQuick
import Quickshell

PanelTitle {
    id: item

    IconSource {
        id: themeIcons
    }

    // False while there is nothing to show; the item then takes no room.
    property bool shown: true

    // The icon's name; empty for an item that is text only.
    property string iconName: ""

    readonly property var statusLooks: panel.looks.status ?? ({})
    readonly property var themePicture: (statusLooks.icons ?? ({}))[iconName]

    // A theme's special gap before this icon (panel.status.gaps).
    readonly property var gapBefore: (statusLooks.gaps ?? ({}))[iconName]

    pictureTinted: true
    pictureSource: {
        if (iconName === "") {
            return ""
        }
        const found = picture(iconName)
        if (found === "") {
            console.warn("status item: neither the theme nor the icon theme has an icon named",
                         iconName, "- check the name, or add a picture to the theme's [panel.status.icons]")
        }
        return found
    }

    // The theme's picture for an icon name, else the icon theme's symbolic
    // icon, else its ordinary one; "" if there is none. Menus use it too.
    function picture(name) {
        const own = (statusLooks.icons ?? ({}))[name]
        if (own !== undefined) {
            return "file://" + panel.themeDirectory + "/" + own
        }
        const oneColour = themeIcons.find(name + "-symbolic")
        return oneColour !== "" ? oneColour : themeIcons.find(name)
    }
    pictureHeight: themePicture !== undefined ? 0 : (statusLooks.icon_size ?? 16)
    textGap: statusLooks.text_gap ?? 4
    // Each item's box reaches half the spacing beyond it.
    boxOverhang: (statusLooks.spacing ?? itemRow.spacing) / 2
}
