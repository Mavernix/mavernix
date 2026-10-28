// Notifications: the shell is the session's notification service. Each
// notification shows as a banner in the screen's top right corner for
// notifications.banner_seconds, then moves to `kept`, the list of past
// notifications. Looks: the theme's [notifications.banner] and [animation.banner].

import QtQuick
import Quickshell
import Quickshell.Wayland
import Quickshell.Services.Notifications
import "ThemeValues.js" as ThemeValues

Scope {
    id: root

    IconSource {
        id: themeIcons
    }

    required property var settings
    // May be null.
    property var sounds: null
    property var screen: null

    readonly property var theme: settings.theme
    readonly property var looks: (theme.notifications ?? ({})).banner ?? ({})
    readonly property var wishes: settings.config.notifications ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    // Each of `arrive` and `leave` (or one for both): { kind, duration_ms, curve,
    // fade }, kind being "none", "fade", "slide" (sideways) or "drop" (from above).
    readonly property var motion: (theme.animation ?? ({})).banner ?? ({ kind: "none" })
    readonly property var arriving: motion.arrive ?? motion
    readonly property var leaving: motion.leave ?? motion
    function durationOf(way) {
        return (way.kind ?? "none") === "none" ? 0 : (way.duration_ms ?? 150)
    }

    property var showing: []

    // Banners that went unanswered, newest first, until they are closed.
    property var kept: []
    // Arrival time in milliseconds since 1970, by notification id.
    property var arrivals: ({})

    function arrivedAt(notification) {
        return arrivals[notification.id] ?? Date.now()
    }

    function keep(notification) {
        kept = [notification].concat(kept)
    }

    function answer(notification) {
        for (const action of notification.actions) {
            if (action.identifier === "default") {
                action.invoke()
            }
        }
        dismiss(notification)
    }

    function dismiss(notification) {
        kept = kept.filter(other => other !== notification)
        notification.dismiss()
    }

    function dismissAll() {
        for (const notification of kept) {
            dismiss(notification)
        }
    }

    function dismissProgram(name) {
        for (const notification of kept.filter(other => other.appName === name)) {
            dismiss(notification)
        }
    }

    function show(notification) {
        if (sounds !== null) {
            sounds.play("message")
        }
        showing = showing.concat([notification])
    }

    function remove(notification) {
        if (showing.includes(notification)) {
            showing = showing.filter(other => other !== notification)
        }
    }

    NotificationServer {
        bodySupported: true
        actionsSupported: true
        imageSupported: true

        onNotification: notification => {
            notification.tracked = true
            root.arrivals[notification.id] = Date.now()
            // Quickshell destroys the notification as soon as this handler returns, so
            // it must leave both lists now: a Repeater over a list holding a destroyed
            // object crashes the shell (a Qt bug).
            notification.closed.connect(() => {
                root.kept = root.kept.filter(other => other !== notification)
                root.remove(notification)
            })
            root.show(notification)
        }
    }

    PanelWindow {
        screen: root.screen
        id: stackWindow

        visible: root.showing.length > 0

        // The banner pictures may have room for a shadow around the panel
        // (background_margins: top, right, bottom, left).
        readonly property var margin: root.looks.background_margins ?? [0, 0, 0, 0]
        readonly property var panelLooks: root.theme.panel ?? ({})
        readonly property bool panelOnTop: (root.settings.config.panel?.position ?? "top") === "top"

        anchors {
            top: true
            right: true
        }
        exclusionMode: ExclusionMode.Ignore
        WlrLayershell.layer: WlrLayer.Overlay
        WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
        margins.top: (panelOnTop ? (panelLooks.height ?? 0) : 0) + (root.looks.top_gap ?? 8) - margin[0]
        margins.right: (root.looks.edge_gap ?? 8) - margin[1]
        color: "transparent"

        implicitWidth: (root.looks.width ?? 320) + margin[1] + margin[3]
        implicitHeight: stack.height + margin[0] + margin[2]

        // Only the banners take the pointer, not their shadows or the gaps between.
        mask: Region {
            item: stack
        }

        Column {
            id: stack
            x: stackWindow.margin[3]
            y: stackWindow.margin[0]
            width: root.looks.width ?? 320
            spacing: root.looks.gap ?? 8

            Repeater {
                // A ScriptModel keeps the banners that stay when the list changes; a
                // Repeater over the list itself rebuilds every banner, restarting its time.
                model: ScriptModel {
                    values: root.showing
                }

                Item {
                    id: banner

                    required property var modelData
                    readonly property var notification: modelData

                    readonly property int textLines: notification.body === "" ? 0 : Math.min(body.lineCount, 2)
                    readonly property var heights: root.looks.heights ?? [44, 60, 76]

                    width: stack.width
                    height: heights[textLines]

                    // `come` runs from 0 to 1 as the banner arrives, `gone` as it leaves.
                    property bool present: false
                    Component.onCompleted: present = true
                    property real come: present || gone > 0 ? 1 : 0
                    property real gone: 0
                    Behavior on come {
                        NumberAnimation {
                            duration: root.durationOf(root.arriving)
                            easing.type: ThemeValues.easing(root.arriving.curve ?? "ease-out")
                        }
                    }
                    NumberAnimation {
                        id: going
                        target: banner
                        property: "gone"
                        to: 1
                        duration: root.durationOf(root.leaving)
                        easing.type: ThemeValues.easing(root.leaving.curve ?? "ease-in")
                    }

                    function share(way, kind, along) {
                        return (way.kind ?? "none") === kind ? along : 0
                    }
                    function fades(way) {
                        return way.kind === "fade" || (way.fade ?? false)
                    }
                    opacity: (fades(root.arriving) ? come : 1) * (fades(root.leaving) ? 1 - gone : 1)
                    transform: Translate {
                        x: (banner.share(root.arriving, "slide", 1 - banner.come) + banner.share(root.leaving, "slide", banner.gone))
                           * (banner.width + stackWindow.margin[1])
                        // Up past the window's top, so the banner comes out from under the panel.
                        y: -(banner.share(root.arriving, "drop", 1 - banner.come) + banner.share(root.leaving, "drop", banner.gone))
                           * (banner.y + banner.height + stackWindow.margin[0])
                    }

                    property bool answered: false
                    function leave(tellProgram) {
                        if (!present) {
                            return
                        }
                        present = false
                        answered = tellProgram
                        stay.stop()
                        // First, so that a banner never stays invisible yet holding its corner.
                        going.start()
                        goodbye.start()
                        if (tellProgram) {
                            for (const action of notification.actions) {
                                if (action.identifier === "default") {
                                    action.invoke()
                                }
                            }
                        }
                    }

                    // The program's own time is in milliseconds (the notification protocol's
                    // unit, though Quickshell's documentation says seconds).
                    Timer {
                        id: stay
                        running: true
                        interval: banner.notification.expireTimeout > 0
                                  ? banner.notification.expireTimeout
                                  : 1000 * (root.wishes.banner_seconds ?? 5)
                        onTriggered: banner.leave(false)
                    }
                    Timer {
                        id: goodbye
                        interval: root.durationOf(root.leaving)
                        onTriggered: {
                            const notification = banner.notification
                            // A pressed banner is done with; an unanswered one goes to `kept`.
                            if (banner.answered) {
                                // Closing the notification removes this banner too.
                                notification.dismiss()
                            } else {
                                root.keep(notification)
                                // Last: this removes the banner.
                                root.remove(notification)
                            }
                        }
                    }

                    Fill {
                        x: -stackWindow.margin[3]
                        y: -stackWindow.margin[0]
                        width: parent.width + stackWindow.margin[1] + stackWindow.margin[3]
                        height: parent.height + stackWindow.margin[0] + stackWindow.margin[2]
                        themeDirectory: root.themeDirectory
                        fill: (root.looks.backgrounds ?? [])[banner.textLines] ?? ({ color: "#f4f4f4" })
                    }

                    Image {
                        id: icon
                        readonly property var inset: root.looks.icon_inset ?? [8, 6]
                        x: inset[0]
                        y: inset[1]
                        width: root.looks.icon_size ?? 32
                        height: width
                        sourceSize: Qt.size(width, height)
                        source: banner.notification.image !== ""
                            ? banner.notification.image
                            : (banner.notification.appIcon !== "" ? themeIcons.find(banner.notification.appIcon) : "")
                    }

                    Text {
                        id: title
                        x: root.looks.text_left ?? 48
                        width: parent.width - x - (root.looks.text_right ?? 12)
                        y: (root.looks.title_baseline_row ?? 20) - baselineOffset
                        elide: Text.ElideRight
                        // Programs may send markup; it is shown as typed.
                        textFormat: Text.PlainText
                        text: banner.notification.summary
                        color: ThemeValues.color(root.looks.title_color ?? "#000000")
                        font.family: root.theme.fonts.banner_title.family
                        font.pixelSize: root.theme.fonts.banner_title.size
                        font.bold: root.theme.fonts.banner_title.bold ?? false
                    }

                    Text {
                        id: body
                        x: title.x
                        width: title.width
                        y: (root.looks.text_baseline_row ?? 36) - baselineOffset
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                        text: banner.notification.body
                        color: ThemeValues.color(root.looks.text_color ?? "#000000")
                        font.family: root.theme.fonts.banner_text.family
                        font.pixelSize: root.theme.fonts.banner_text.size
                        font.bold: root.theme.fonts.banner_text.bold ?? false
                        lineHeightMode: Text.FixedHeight
                        lineHeight: root.looks.text_line_height ?? 14
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: banner.leave(true)
                    }
                }
            }
        }
    }
}
