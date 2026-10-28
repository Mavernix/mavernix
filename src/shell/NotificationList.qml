// The list of past notifications: a panel sliding in at the screen's side,
// grouped by program, holding those whose banners went unanswered.
// `toggle()` opens and closes it; Banners.qml keeps the notifications.
// Theme: [notifications.list], [animation.notification_list]; wording:
// the config's [notifications].

import QtQuick
import Quickshell
import Quickshell.Wayland
import "ThemeValues.js" as ThemeValues

PanelWindow {
    id: list

    IconSource {
        id: themeIcons
    }

    required property var settings
    required property var banners

    readonly property var theme: settings.theme
    readonly property var looks: (theme.notifications ?? ({})).list ?? ({})
    readonly property var words: settings.config.notifications ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    readonly property var motion: (theme.animation ?? ({})).notification_list ?? ({ kind: "none" })

    property bool open: false
    function toggle() {
        open = !open
    }

    property real shown: open ? 1 : 0
    Behavior on shown {
        NumberAnimation {
            duration: list.motion.kind === "none" ? 0 : (list.motion.duration_ms ?? 200)
            easing.type: ThemeValues.easing(list.motion.curve)
        }
    }
    visible: shown > 0

    // Ignores reserved room (or it would end above the dock); shell.qml keeps
    // the dock in front of it while it shows.
    anchors {
        top: true
        bottom: true
        right: true
    }
    margins.top: theme.panel?.height ?? 0
    implicitWidth: looks.width ?? 300
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    color: "transparent"

    // [{ name, icon, notifications }], one per program.
    readonly property var sections: {
        const byProgram = []
        for (const notification of banners.kept) {
            let section = byProgram.find(other => other.name === notification.appName)
            if (section === undefined) {
                section = { name: notification.appName, icon: notification.appIcon, notifications: [] }
                byProgram.push(section)
            }
            section.notifications.push(notification)
        }
        return byProgram
    }

    function ago(moment, now) {
        const minutes = Math.floor((now - moment) / 60000)
        if (minutes < 1) {
            return words.time_now ?? "now"
        }
        if (minutes < 60) {
            return (words.time_minutes ?? "%1m ago").arg(minutes)
        }
        return (words.time_hours ?? "%1h ago").arg(Math.floor(minutes / 60))
    }
    SystemClock {
        id: clock
        precision: SystemClock.Minutes
    }

    component ListText: Text {
        color: ThemeValues.color(list.looks.text_color ?? "#ffffff")
        font.family: list.theme.fonts.notification_list.family
        font.pixelSize: list.theme.fonts.notification_list.size
        elide: Text.ElideRight
    }

    Item {
        id: slab
        width: parent.width
        height: parent.height
        x: (1 - list.shown) * width

        Fill {
            anchors.fill: parent
            fill: list.looks.background ?? ({ color: "#303030" })
            themeDirectory: list.themeDirectory
        }
        // An optional edge shadow, colours listed from the edge inwards.
        Row {
            Repeater {
                model: list.looks.edge_shadow ?? []
                Rectangle {
                    required property string modelData
                    width: 1
                    height: slab.height
                    color: ThemeValues.color(modelData)
                }
            }
        }

        ListText {
            visible: list.sections.length === 0
            anchors.horizontalCenter: parent.horizontalCenter
            y: (list.looks.empty_text_top ?? 280)
            text: list.words.none_label ?? "No notifications"
            font.bold: true
        }

        Flickable {
            anchors.fill: parent
            contentHeight: column.height
            clip: true
            visible: list.sections.length > 0

            Column {
                id: column
                width: parent.width

                Item {
                    width: parent.width
                    height: list.looks.heading_height ?? 22
                    ListText {
                        x: list.looks.text_left ?? 30
                        anchors.verticalCenter: parent.verticalCenter
                        text: list.words.list_title ?? "Notifications"
                        color: ThemeValues.color(list.looks.heading_color ?? list.looks.text_color ?? "#ffffff")
                        font.pixelSize: list.theme.fonts.notification_list_heading.size
                        font.bold: list.theme.fonts.notification_list_heading.bold ?? true
                    }
                    // Takes every notification away at once.
                    ListText {
                        anchors.right: parent.right
                        anchors.rightMargin: list.looks.time_right ?? 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: list.words.clear_all_label ?? "Clear All"
                        color: ThemeValues.color(list.looks.heading_color ?? list.looks.text_color ?? "#ffffff")
                        opacity: clearAll.pressed ? 0.5 : (clearAll.containsMouse ? 1 : 0.8)
                        font.underline: clearAll.containsMouse
                        MouseArea {
                            id: clearAll
                            anchors.fill: parent
                            anchors.margins: -4
                            hoverEnabled: true
                            onClicked: list.banners.dismissAll()
                        }
                    }
                }

                Repeater {
                    model: list.sections

                    Column {
                        id: section
                        required property var modelData
                        width: column.width

                        Item {
                            width: parent.width
                            height: list.looks.section_height ?? 24
                            Fill {
                                anchors.fill: parent
                                fill: list.looks.section_background ?? ({ color: "#00000040" })
                                themeDirectory: list.themeDirectory
                            }
                            Image {
                                x: list.looks.icon_left ?? 10
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16
                                height: 16
                                sourceSize: Qt.size(16, 16)
                                source: section.modelData.icon !== "" ? themeIcons.find(section.modelData.icon) : ""
                            }
                            ListText {
                                x: list.looks.text_left ?? 30
                                width: parent.width - x - 40
                                anchors.verticalCenter: parent.verticalCenter
                                text: section.modelData.name
                                font.pixelSize: list.theme.fonts.notification_list_heading.size
                                font.bold: list.theme.fonts.notification_list_heading.bold ?? true
                            }
                            Item {
                                id: closeButton
                                readonly property var picture: list.looks.close_image
                                width: 24
                                height: parent.height
                                x: parent.width - width - (list.looks.close_right ?? 6)
                                Image {
                                    visible: closeButton.picture !== undefined
                                    anchors.centerIn: parent
                                    source: closeButton.picture !== undefined
                                        ? "file://" + list.themeDirectory + "/"
                                          + (closeArea.pressed ? (list.looks.close_pressed_image ?? closeButton.picture)
                                                               : closeButton.picture)
                                        : ""
                                }
                                ListText {
                                    visible: closeButton.picture === undefined
                                    anchors.centerIn: parent
                                    text: "×"
                                    opacity: closeArea.pressed ? 0.5 : 1
                                }
                                MouseArea {
                                    id: closeArea
                                    anchors.fill: parent
                                    onClicked: list.banners.dismissProgram(section.modelData.name)
                                }
                            }
                        }

                        Repeater {
                            model: section.modelData.notifications

                            Item {
                                id: entry
                                required property var modelData
                                width: section.width
                                height: body.y + (body.text !== "" ? body.height : 0)
                                        + (list.looks.entry_bottom_padding ?? 6)

                                Rectangle {
                                    visible: list.looks.divider_color !== undefined
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 1
                                    color: ThemeValues.color(list.looks.divider_color)
                                }
                                Image {
                                    visible: list.looks.unread_image !== undefined
                                    x: list.looks.unread_left ?? 12
                                    y: list.looks.unread_top ?? 8
                                    source: visible ? "file://" + list.themeDirectory + "/" + list.looks.unread_image : ""
                                }
                                ListText {
                                    id: title
                                    x: list.looks.text_left ?? 30
                                    y: list.looks.title_top ?? 2
                                    width: parent.width - x - 60
                                    text: entry.modelData.summary
                                    font.bold: true
                                }
                                ListText {
                                    // Under the pointer, the time gives way to a close button.
                                    visible: !entryHover.hovered
                                    anchors.right: parent.right
                                    anchors.rightMargin: list.looks.time_right ?? 10
                                    y: (list.looks.title_top ?? 2) + 3
                                    text: list.ago(list.banners.arrivedAt(entry.modelData), clock.date.getTime())
                                    font.pixelSize: list.theme.fonts.notification_list_time.size
                                    font.bold: list.theme.fonts.notification_list_time.bold ?? true
                                }
                                ListText {
                                    id: body
                                    x: title.x
                                    y: title.y + (list.looks.line_height ?? 17)
                                    width: parent.width - x - (list.looks.time_right ?? 10)
                                    text: entry.modelData.body
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 3
                                    lineHeightMode: Text.FixedHeight
                                    lineHeight: list.looks.line_height ?? 17
                                }
                                HoverHandler {
                                    id: entryHover
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: list.banners.answer(entry.modelData)
                                }
                                ListText {
                                    visible: entryHover.hovered
                                    anchors.right: parent.right
                                    anchors.rightMargin: list.looks.time_right ?? 10
                                    y: list.looks.title_top ?? 2
                                    text: "×"
                                    font.bold: true
                                    opacity: entryClose.pressed ? 0.5 : 1
                                    MouseArea {
                                        id: entryClose
                                        anchors.fill: parent
                                        anchors.margins: -6
                                        onClicked: list.banners.dismiss(entry.modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
