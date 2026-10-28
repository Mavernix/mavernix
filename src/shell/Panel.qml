// The panel: a bar along one edge of the screen. Its looks come from the
// theme's [panel] table and [fonts].panel, its contents and position from
// the config's [panel] table.

import QtQuick
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: panel

    // Lets the compositor tell this surface apart (to keep it still while
    // desktops slide, say).
    WlrLayershell.namespace: "desktop-panel"

    required property var settings
    property var confirmDialog: null
    property var outputVolume: null
    // The shell's root, which items can ask to `run(action)`.
    property var shell: null
    property var extensions: null
    // The shell's UserChoices, for what the user changes from the panel; may be null.
    property var choices: null
    // A panel end's items: the config's, with extensions' items added on the
    // inner side; an item the config lists itself stays where it is.
    function itemsAt(end) {
        const listed = layout[end] ?? []
        const added = (extensions?.panelItems(end) ?? []).filter(name => !listed.includes(name))
        return end === "right" ? added.concat(listed) : listed.concat(added)
    }
    // The native ApplicationMenu for the "menus" item; null for no menus.
    property var applicationMenu: null

    readonly property var looks: settings.theme.panel
    readonly property var font: settings.theme.fonts.panel
    readonly property var layout: settings.config.panel
    readonly property string themeDirectory: settings.theme_directory

    // One-pixel lines along the panel's inner edge (panel.separator).
    readonly property var separatorColors: looks.separator ?? []

    // One-pixel lines along the outer edge, over the fill (panel.highlight).
    readonly property var highlightColors: looks.highlight ?? []

    readonly property bool atBottom: layout.position === "bottom"

    anchors {
        top: !panel.atBottom
        bottom: panel.atBottom
        left: true
        right: true
    }
    implicitHeight: looks.height

    color: "transparent"

    // Hides the panel's contents and input but keeps its reserved strip, so
    // windows do not move (for full-screen overlays such as the app grid).
    property bool concealed: false
    contentItem.opacity: concealed ? 0 : 1
    Region {
        id: nothing
    }
    mask: concealed ? nothing : null

    Fill {
        id: background
        fill: panel.looks.background
        themeDirectory: panel.themeDirectory
        anchors {
            left: parent.left
            right: parent.right
            top: panel.atBottom ? separator.bottom : parent.top
            bottom: panel.atBottom ? parent.bottom : separator.top
        }
    }

    Column {
        id: separator
        anchors {
            left: parent.left
            right: parent.right
            top: panel.atBottom ? parent.top : undefined
            bottom: panel.atBottom ? undefined : parent.bottom
        }

        Repeater {
            // Listed from the line next to the desktop; reversed for a top panel,
            // since a column fills downwards.
            model: panel.atBottom ? panel.separatorColors
                                  : panel.separatorColors.slice().reverse()
            Rectangle {
                required property string modelData
                width: separator.width
                height: 1
                color: ThemeValues.color(modelData)
            }
        }
    }

    Column {
        id: highlight
        anchors {
            left: parent.left
            right: parent.right
            top: panel.atBottom ? undefined : parent.top
            bottom: panel.atBottom ? parent.bottom : undefined
        }

        Repeater {
            // Listed from the screen's edge; reversed for a bottom panel.
            model: panel.atBottom ? panel.highlightColors.slice().reverse()
                                  : panel.highlightColors
            Rectangle {
                required property string modelData
                width: highlight.width
                height: 1
                color: ThemeValues.color(modelData)
            }
        }
    }

    PanelItems {
        anchors {
            left: parent.left
            leftMargin: panel.looks.edge_padding
            top: background.top
            bottom: background.bottom
        }
        names: panel.itemsAt("left")
        panel: panel
    }

    PanelItems {
        anchors {
            right: parent.right
            rightMargin: panel.looks.edge_padding
            top: background.top
            bottom: background.bottom
        }
        names: panel.itemsAt("right")
        panel: panel
    }

    Loader {
        active: panel.looks.shadow !== undefined && !panel.concealed
        sourceComponent: PanelShadow {
            screen: panel.screen
            forPanel: panel
        }
    }

    // --- Menus opened from titles ----------------------------------------------

    // The PanelTitle whose menu is open, or null.
    property var openTitle: null

    // A menu belongs to the window active when it opened; close it when that changes.
    Connections {
        target: ToplevelManager
        function onActiveToplevelChanged() {
            panel.closeMenu()
        }
    }

    // Runs a config menu entry: one of the shell's actions (`action`, such as
    // "lock"), or its command, asking first if it has a `confirm` table.
    function runEntry(entry) {
        if (entry.action !== undefined) {
            panel.shell?.run(entry.action)
        } else if (confirmDialog !== null) {
            confirmDialog.run(entry)
        } else {
            Quickshell.execDetached(entry.command)
        }
    }

    function openMenu(title) {
        title.opening()
        openTitle = title
    }

    function closeMenu() {
        openTitle = null
    }

    // Registered by each PanelTitle, for moving between menus by key.
    property var titles: []
    function registerTitle(title) {
        titles = titles.concat([title])
    }
    function forgetTitle(title) {
        titles = titles.filter(other => other !== title)
    }

    // Opens the next (1) or previous (-1) title's menu, wrapping at the ends.
    function openNeighbour(direction) {
        if (openTitle === null) {
            return
        }
        // Only among titles at the same end, so the keys never wander from the
        // menus onto the status items.
        const withMenus = titles
            .filter(title => title.itemRow === openTitle.itemRow)
            .filter(title => title.visible && title.width > 0 && title.menuItems.length > 0)
            .sort((first, second) => first.mapToItem(null, 0, 0).x - second.mapToItem(null, 0, 0).x)
        const now = withMenus.indexOf(openTitle)
        if (now < 0 || withMenus.length < 2) {
            return
        }
        openMenu(withMenus[(now + direction + withMenus.length) % withMenus.length])
    }

    function toggleMenu(title) {
        if (openTitle === title) {
            closeMenu()
        } else {
            openMenu(title)
        }
    }

    MenuOverlay {
        screen: panel.screen
        settings: panel.settings
        visible: panel.openTitle !== null
        items: panel.openTitle?.menuItems ?? []

        // Hangs from the panel's inner edge, aligned with the title's box.
        menuX: panel.openTitle ? panel.openTitle.mapToItem(null, 0, 0).x - panel.openTitle.boxOverhang : 0
        menuY: panel.atBottom ? screen.height - panel.height - menuHeight : panel.height
        squareTop: !panel.atBottom

        // The panel stays reachable, so hovering another title switches menus.
        passX: 0
        passY: panel.atBottom ? screen.height - panel.height : 0
        passWidth: panel.width
        passHeight: panel.height

        onChosen: id => {
            const title = panel.openTitle
            panel.closeMenu()
            title.chosen(id)
        }
        onSubmenuWanted: id => panel.applicationMenu?.aboutToShow(id)
        onDismissed: panel.closeMenu()
        onSlid: (id, value) => panel.openTitle?.slid(id, value)
        onNeighbourWanted: direction => panel.openNeighbour(direction)
    }
}
