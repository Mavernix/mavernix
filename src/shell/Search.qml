// Search: a field that drops from the panel, listing applications and home
// folder files matching the name typed, then any web searches from the
// config. `toggle()` opens and closes it. Theme: [search]; wording, folders
// and web searches: the config's [search].
//
// Files are found with `find`, restarted shortly after each change, not
// through an index.

import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import Shell.Native
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: search

    IconSource {
        id: themeIcons
        choices: search.choices
    }

    // UserChoices, for user-chosen app icons; may be null.
    property var choices: null

    required property var settings

    readonly property var theme: settings.theme
    readonly property var looks: theme.search ?? ({})
    readonly property var menuLooks: theme.menu
    readonly property var wishes: settings.config.search ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    property bool open: false
    function toggle() {
        open = !open
    }
    onOpenChanged: {
        field.text = ""
        marked = 0
        if (open) {
            field.forceActiveFocus()
        }
    }

    visible: open

    // Full-screen and keyboard-grabbing; a press outside the box closes it.
    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: open ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: search.open = false
    }

    // --- What is found ----------------------------------------------------------

    readonly property string wanted: field.text.trim()
    readonly property string wantedLowerCase: wanted.toLowerCase()
    readonly property int mostOfAKind: wishes.most_of_a_kind ?? 5
    // One letter is in nearly every name, so files wait for this many.
    readonly property int fewestLettersForFiles: 2

    // Names starting with the text come first, then shorter ones.
    function closerMatchFirst(firstName, secondName) {
        const firstBegins = firstName.toLowerCase().startsWith(wantedLowerCase)
        const secondBegins = secondName.toLowerCase().startsWith(wantedLowerCase)
        if (firstBegins !== secondBegins) {
            return firstBegins ? -1 : 1
        }
        if (firstName.length !== secondName.length) {
            return firstName.length - secondName.length
        }
        return firstName.localeCompare(secondName)
    }

    readonly property var foundApplications: {
        if (wanted === "") {
            return []
        }
        const matches = DesktopEntries.applications.values.filter(
            entry => !entry.noDisplay && entry.name.toLowerCase().includes(search.wantedLowerCase))
        matches.sort((first, second) => search.closerMatchFirst(first.name, second.name))
        return matches.slice(0, mostOfAKind)
    }

    // Paths from `find`; folders end with a slash.
    property var foundPaths: []

    // Debounced, so typing a word does not start a search per letter.
    Timer {
        id: afterTyping
        interval: 250
        onTriggered: {
            finder.running = false
            if (search.wanted.length >= search.fewestLettersForFiles) {
                finder.running = true
            } else {
                search.foundPaths = []
            }
        }
    }
    onWantedChanged: {
        marked = 0
        afterTyping.restart()
    }

    Process {
        id: finder
        // Under the configured folders (home if none), skipping hidden ones, a few
        // levels deep. Characters special to -iname are kept out of the pattern.
        readonly property var folders: (search.wishes.folders ?? ["~"]).map(
            folder => folder.startsWith("~") ? Quickshell.env("HOME") + folder.substring(1) : folder)
        readonly property string pattern: "*" + search.wanted.replace(/[*?\[\]\\]/g, "") + "*"
        command: ["find"].concat(folders).concat([
            "-mindepth", "1", "-maxdepth", String(search.wishes.depth ?? 4),
            "-name", ".*", "-prune", "-o",
            "-iname", pattern,
            "(", "-type", "d", "-printf", "%p/\\n", "-o", "-type", "f", "-print", ")"])
        stdout: StdioCollector {
            onStreamFinished: search.foundPaths = text.split("\n").filter(line => line !== "")
        }
    }

    function lastPart(path) {
        const trimmed = path.endsWith("/") ? path.slice(0, -1) : path
        return trimmed.substring(trimmed.lastIndexOf("/") + 1)
    }

    function closestPaths(folders) {
        const paths = foundPaths.filter(path => path.endsWith("/") === folders)
        paths.sort((first, second) => closerMatchFirst(lastPart(first), lastPart(second)))
        return paths.slice(0, mostOfAKind)
    }

    FileKind {
        id: fileKinds
    }

    // The file type's icon, else its family's, else a plain document.
    function fileIcon(path) {
        const icons = fileKinds.iconsFor(path)
        const own = themeIcons.find(icons.icon)
        return own !== "" ? own : themeIcons.path(icons.genericIcon, "text-x-generic")
    }

    function openPath(path) {
        Quickshell.execDetached((wishes.open_command ?? ["xdg-open"]).concat([path]))
    }

    // Rows { heading, name, icon, open() }; heading only on a kind's first row.
    readonly property var rows: {
        if (wanted === "") {
            return []
        }

        const result = []
        const add = (heading, entries) => entries.forEach((entry, number) => {
            result.push(Object.assign({}, entry, { heading: number === 0 ? heading : "" }))
        })

        add(wishes.applications_label ?? "Applications", foundApplications.map(entry => ({
            name: entry.name,
            icon: themeIcons.forEntry(entry, "application-x-executable"),
            open: () => entry.execute()
        })))
        add(wishes.files_label ?? "Files", closestPaths(false).map(path => ({
            name: lastPart(path),
            icon: fileIcon(path),
            open: () => search.openPath(path)
        })))
        add(wishes.folders_label ?? "Folders", closestPaths(true).map(path => ({
            name: lastPart(path),
            icon: themeIcons.path("folder"),
            open: () => search.openPath(path)
        })))

        // The top hit is repeated first if the config has a heading for it.
        if (result.length > 0 && (wishes.top_hit_label ?? "") !== "") {
            result.unshift(Object.assign({}, result[0], { heading: wishes.top_hit_label }))
        }

        // "%s" in a web search's label and address stands for the text typed.
        const typed = wanted
        add(wishes.web_label ?? "Web", (wishes.web ?? []).map(entry => ({
            name: entry.label.replace("%s", typed),
            icon: themeIcons.path(entry.icon ?? "applications-internet", "text-html"),
            open: () => search.openPath(entry.address.replace("%s", encodeURIComponent(typed)))
        })))
        return result
    }

    property int marked: 0

    function openMarked() {
        const row = rows[marked]
        if (row !== undefined) {
            row.open()
            open = false
        }
    }

    // --- What is shown ----------------------------------------------------------

    // A font from this part's theme table, else the menus' font.
    component SearchText: Text {
        property var face: search.theme.fonts.search ?? search.theme.fonts.menu
        font.family: face.family
        font.pixelSize: face.size
        font.bold: face.bold ?? false
        elide: Text.ElideRight
    }

    readonly property real headingWidth: looks.heading_width ?? 110
    readonly property real rowHeight: looks.row_height ?? 22
    readonly property real iconSize: looks.icon_size ?? 16
    // Drawn like a menu's marked item unless the theme gives its own colours.
    readonly property var markFill: looks.highlight ?? menuLooks.highlight
    readonly property color markTextColor: ThemeValues.color(
        looks.highlight_text_color ?? menuLooks.highlight_text_color)

    Item {
        id: box
        width: search.looks.width ?? 420
        x: search.width - width - (search.looks.right_inset ?? 12)
        y: search.theme.panel?.height ?? 0
        height: header.height + results.height

        // Keeps presses on the box from closing it.
        MouseArea {
            anchors.fill: parent
        }
        BoxFrame {
            looks: Object.assign({}, search.menuLooks, search.looks.box ?? ({}))
            squareTopLeft: true
            squareTopRight: true
        }

        Rectangle {
            visible: search.looks.heading_column_color !== undefined
            y: header.height
            width: search.headingWidth
            height: results.height
            bottomLeftRadius: search.menuLooks.corner_radius
            color: ThemeValues.color(search.looks.heading_column_color ?? "#00000000")
        }
        Rectangle {
            visible: search.looks.divider_color !== undefined
            x: search.headingWidth
            y: header.height
            width: 1
            height: results.height
            color: ThemeValues.color(search.looks.divider_color ?? "#00000000")
        }

        Item {
            id: header
            width: parent.width
            height: search.looks.header_height ?? 32

            Fill {
                anchors.fill: parent
                fill: search.looks.header_background ?? ({ color: "#00000000" })
                themeDirectory: search.themeDirectory
            }
            Rectangle {
                visible: search.looks.header_line_color !== undefined
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: ThemeValues.color(search.looks.header_line_color ?? "#00000000")
            }
            // The label, with an optional shadow one row lower.
            SearchText {
                visible: search.looks.header_text_shadow_color !== undefined
                face: label.face
                width: label.width
                horizontalAlignment: Text.AlignRight
                y: label.y + 1
                text: label.text
                color: ThemeValues.color(search.looks.header_text_shadow_color ?? "#00000000")
            }
            SearchText {
                id: label
                face: search.theme.fonts.search_label ?? search.theme.fonts.menu
                width: search.headingWidth - (search.looks.label_gap ?? 8)
                horizontalAlignment: Text.AlignRight
                anchors.verticalCenter: parent.verticalCenter
                text: search.wishes.label ?? ""
                color: ThemeValues.color(search.looks.header_text_color ?? "#ffffff")
            }

            Item {
                id: fieldBox
                x: search.headingWidth + (search.looks.field_inset ?? 10)
                width: parent.width - x - (search.looks.field_right ?? 8)
                height: search.looks.field_height ?? 22
                y: search.looks.field_top ?? Math.round((header.height - height) / 2)

                Fill {
                    anchors.fill: parent
                    fill: search.looks.field ?? ({ color: "#ffffff", radius: 4 })
                    themeDirectory: search.themeDirectory
                }
                TextInput {
                    id: field
                    x: search.looks.field_text_inset ?? 10
                    width: clearButton.x - x
                    // Centred unless the theme gives the baseline row.
                    y: search.looks.field_text_baseline_row !== undefined
                        ? Math.round(search.looks.field_text_baseline_row - baselineOffset)
                        : Math.round((fieldBox.height - height) / 2)
                    clip: true
                    color: ThemeValues.color(search.looks.field_text_color ?? "#000000")
                    readonly property var face: search.theme.fonts.search ?? search.theme.fonts.menu
                    font.family: face.family
                    font.pixelSize: face.size

                    Keys.onEscapePressed: search.open = false
                    Keys.onDownPressed: search.marked = Math.min(search.rows.length - 1, search.marked + 1)
                    Keys.onUpPressed: search.marked = Math.max(0, search.marked - 1)
                    onAccepted: search.openMarked()
                }

                // Empties the field; shown while there is text.
                Rectangle {
                    id: clearButton
                    readonly property var buttonLooks: search.looks.clear_button ?? ({})
                    readonly property real crossLength: buttonLooks.cross_length ?? 7
                    visible: field.text !== ""
                    width: buttonLooks.size ?? 14
                    height: width
                    radius: width / 2
                    x: parent.width - width - (buttonLooks.right_inset ?? 4)
                    anchors.verticalCenter: parent.verticalCenter
                    color: ThemeValues.color(buttonLooks.color ?? "#b0b0b0")

                    Repeater {
                        model: [45, -45]
                        Rectangle {
                            required property int modelData
                            anchors.centerIn: parent
                            width: clearButton.crossLength
                            height: clearButton.buttonLooks.cross_thickness ?? 1.5
                            rotation: modelData
                            antialiasing: true
                            color: ThemeValues.color(clearButton.buttonLooks.cross_color ?? "#ffffff")
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: field.text = ""
                    }
                }
            }
        }

        // A new kind begins after a larger gap than a row of the same kind.
        Column {
            id: results
            y: header.height
            width: parent.width
            topPadding: search.rows.length > 0 ? (search.looks.results_padding ?? 6) : 0
            bottomPadding: topPadding

            Repeater {
                model: search.rows

                Item {
                    id: row
                    required property int index
                    required property var modelData
                    readonly property bool isMarked: index === search.marked
                    readonly property real gapAbove: {
                        if (index === 0) {
                            return 0
                        }
                        return modelData.heading !== "" ? (search.looks.kind_gap ?? 10)
                                                        : (search.looks.row_gap ?? 0)
                    }
                    width: results.width
                    height: search.rowHeight + gapAbove

                    Item {
                        y: row.gapAbove
                        width: parent.width
                        height: search.rowHeight

                        Fill {
                            anchors.fill: parent
                            visible: row.isMarked
                            fill: search.markFill
                            themeDirectory: search.themeDirectory
                        }
                        SearchText {
                            face: search.theme.fonts.search_heading ?? search.theme.fonts.menu
                            width: search.headingWidth - (search.looks.heading_gap ?? 8)
                            horizontalAlignment: Text.AlignRight
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.modelData.heading
                            color: row.isMarked ? search.markTextColor
                                : ThemeValues.color(search.looks.heading_color ?? "#808080")
                        }
                        Image {
                            x: search.headingWidth + (search.looks.icon_inset ?? 9)
                            anchors.verticalCenter: parent.verticalCenter
                            width: search.iconSize
                            height: search.iconSize
                            sourceSize: Qt.size(search.iconSize, search.iconSize)
                            source: row.modelData.icon
                        }
                        SearchText {
                            x: search.headingWidth + (search.looks.name_inset ?? 33)
                            width: parent.width - x - (search.looks.name_right ?? 10)
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.modelData.name
                            color: row.isMarked ? search.markTextColor
                                : ThemeValues.color(search.menuLooks.text_color)
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onPositionChanged: search.marked = row.index
                            onClicked: {
                                search.marked = row.index
                                search.openMarked()
                            }
                        }
                    }
                }
            }
        }
    }
}
