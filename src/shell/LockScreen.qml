// The lock screen. Locking itself is the compositor's (ext-session-lock):
// once locked, the screens stay locked even if the shell dies. The password
// is checked through PAM. Theme: [lock]; wording and PAM service: the
// config's [lock].
//
// The lock is also noted in a file, so that a shell restarted while locked
// (src/session/keep-running) locks again instead of leaving screens blank.

import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import Quickshell.Services.Pam
import "ThemeValues.js" as ThemeValues

Item {
    id: lockScreen

    IconSource {
        id: themeIcons
    }

    required property var settings
    // What the desktop shows behind its windows.
    property var desktopFill: settings.theme.desktop.background

    readonly property var theme: settings.theme
    readonly property var looks: theme.lock ?? ({})
    readonly property var wishes: settings.config.lock ?? ({})
    readonly property var user: settings.user ?? ({})
    readonly property string themeDirectory: settings.theme_directory

    property bool locked: false
    // True once the compositor shows nothing but the lock screen.
    readonly property bool secure: sessionLock.secure
    // "" (nothing yet), "wrong", or a message from PAM to show in the field.
    property string problem: ""
    // Counts wrong tries, each of which triggers the theme's shake.
    property int wrongTries: 0

    function lock() {
        locked = true
    }

    function unlock() {
        problem = ""
        locked = false
    }

    // --- Remembering the lock across a restart of the shell --------------------

    FileView {
        id: lockedNote
        path: Quickshell.env("XDG_RUNTIME_DIR") + "/mavernix-locked"
        blockLoading: true
        printErrors: false
    }
    onLockedChanged: lockedNote.setText(locked ? "locked\n" : "")
    Component.onCompleted: {
        if (lockedNote.text().trim() === "locked") {
            locked = true
        }
    }

    // --- Checking the password ------------------------------------------------

    PamContext {
        id: pam
        // A file in /etc/pam.d/; the config may point at another folder.
        config: lockScreen.wishes.pam_service ?? "login"
        configDirectory: lockScreen.wishes.pam_config_directory ?? "/etc/pam.d"

        // Kept only until PAM asks for it.
        property string typed: ""

        onPamMessage: {
            if (responseRequired) {
                respond(typed)
                typed = ""
            } else if (messageIsError) {
                lockScreen.problem = message
            }
        }
        onCompleted: result => {
            typed = ""
            if (result === PamResult.Success) {
                lockScreen.unlock()
            } else {
                lockScreen.problem = "wrong"
                lockScreen.wrongTries += 1
            }
        }
        // No verdict counts as a wrong try (some PAM setups answer a wrong
        // password this way); the reason goes to the log.
        onError: error => {
            typed = ""
            console.warn("lock screen: the password could not be checked:", PamError.toString(error),
                         "- PAM service", config, "in", configDirectory)
            lockScreen.problem = "wrong"
            lockScreen.wrongTries += 1
        }
    }

    function tryPassword(password) {
        if (pam.active || password === "") {
            return
        }
        problem = ""
        pam.typed = password
        pam.start()
    }

    // --- What is shown ---------------------------------------------------------

    WlSessionLock {
        id: sessionLock
        locked: lockScreen.locked

        WlSessionLockSurface {
            id: surface
            color: "black"

            Fill {
                anchors.fill: parent
                fill: lockScreen.desktopFill
                themeDirectory: lockScreen.themeDirectory
            }
            Fill {
                anchors.fill: parent
                fill: lockScreen.looks.backdrop_veil ?? ({ color: "#00000080" })
                themeDirectory: lockScreen.themeDirectory
            }

            Text {
                readonly property var clockLooks: lockScreen.looks.clock ?? ({})
                visible: lockScreen.looks.clock !== undefined
                anchors.right: parent.right
                anchors.rightMargin: clockLooks.right ?? 24
                y: (clockLooks.baseline ?? 18) - baselineOffset
                color: ThemeValues.color(clockLooks.color ?? "#ffffff")
                font.family: lockScreen.theme.fonts.interface.family
                font.pixelSize: lockScreen.theme.fonts.interface.size
                text: Qt.formatDateTime(clock.date, lockScreen.settings.config.panel?.clock_format ?? "hh:mm")
                SystemClock {
                    id: clock
                    precision: SystemClock.Minutes
                }
            }

            Item {
                id: box
                readonly property var boxLooks: lockScreen.looks.box ?? ({})
                width: boxLooks.width ?? 260
                height: boxLooks.height ?? 240
                // Its centre is a share of the screen's height down.
                readonly property real restX: Math.round((surface.width - width) / 2)
                property real shakeOffset: 0
                x: restX + shakeOffset
                y: Math.round(surface.height * (boxLooks.center_y ?? 0.5) - height / 2)

                RectangularShadow {
                    visible: box.boxLooks.shadow !== undefined
                    anchors.fill: parent
                    radius: box.boxLooks.fill?.radius ?? 0
                    color: ThemeValues.color(box.boxLooks.shadow?.color)
                    blur: box.boxLooks.shadow?.blur ?? 0
                    offset: Qt.vector2d(box.boxLooks.shadow?.offset[0] ?? 0, box.boxLooks.shadow?.offset[1] ?? 0)
                }
                Fill {
                    anchors.fill: parent
                    fill: box.boxLooks.fill ?? ({ color: "#f0f0f0", radius: 6 })
                    themeDirectory: lockScreen.themeDirectory
                }

                readonly property real pictureSize: lockScreen.looks.picture?.size ?? 80
                Image {
                    id: picture
                    visible: false
                    width: box.pictureSize
                    height: box.pictureSize
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectCrop
                    source: lockScreen.user.picture
                        ? "file://" + lockScreen.user.picture
                        : themeIcons.path("avatar-default", "user-identity")
                }
                Rectangle {
                    id: pictureShape
                    visible: false
                    layer.enabled: true
                    width: box.pictureSize
                    height: box.pictureSize
                    radius: (lockScreen.looks.picture?.round ?? true) ? width / 2 : 0
                }
                MultiEffect {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: lockScreen.looks.picture?.top ?? 30
                    width: box.pictureSize
                    height: box.pictureSize
                    source: picture
                    maskEnabled: true
                    maskSource: pictureShape
                    // A one-pixel soft edge instead of a jagged one.
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 1.0
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: (lockScreen.looks.name_baseline ?? 140) - baselineOffset
                    text: lockScreen.user.full_name || lockScreen.user.name || ""
                    color: ThemeValues.color(lockScreen.looks.text_color ?? "#000000")
                    font.family: lockScreen.theme.fonts.lock_name.family
                    font.pixelSize: lockScreen.theme.fonts.lock_name.size
                    font.bold: lockScreen.theme.fonts.lock_name.bold ?? false
                }

                Item {
                    id: field
                    readonly property var fieldLooks: lockScreen.looks.field ?? ({})
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: fieldLooks.top ?? 180
                    width: fieldLooks.width ?? 160
                    height: fieldLooks.height ?? 22

                    // Focus glow rings, from the field's edge outwards.
                    Repeater {
                        model: field.fieldLooks.focus_rings ?? []
                        Rectangle {
                            required property int index
                            required property string modelData
                            anchors.fill: parent
                            anchors.margins: -(index + 1)
                            color: "transparent"
                            border.width: 1
                            border.color: ThemeValues.color(modelData)
                            radius: (field.fieldLooks.fill?.radius ?? 0) + index + 1
                        }
                    }
                    Fill {
                        anchors.fill: parent
                        fill: field.fieldLooks.fill ?? ({ color: "#ffffff", border: { width: 1, color: "#909090" } })
                        themeDirectory: lockScreen.themeDirectory
                    }
                    Text {
                        visible: password.text === ""
                        x: field.fieldLooks.text_inset ?? 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: lockScreen.problem !== "" && lockScreen.problem !== "wrong"
                            ? lockScreen.problem
                            : (lockScreen.wishes.password_placeholder ?? "Password")
                        color: ThemeValues.color(field.fieldLooks.placeholder_color ?? "#808080")
                        font: password.font
                    }
                    TextInput {
                        id: password
                        x: field.fieldLooks.text_inset ?? 4
                        width: parent.width - 2 * x
                        anchors.verticalCenter: parent.verticalCenter
                        clip: true
                        focus: true
                        echoMode: TextInput.Password
                        // Never show the last typed character.
                        passwordMaskDelay: 0
                        enabled: !pam.active
                        color: ThemeValues.color(lockScreen.looks.text_color ?? "#000000")
                        font.family: lockScreen.theme.fonts.interface.family
                        font.pixelSize: lockScreen.theme.fonts.interface.size
                        onAccepted: {
                            lockScreen.tryPassword(text)
                            text = ""
                        }
                    }
                }

                readonly property var wrongMotion:
                    (lockScreen.theme.animation ?? ({})).lock_wrong ?? ({ kind: "none" })
                // The shake: equal-length half-swings to either side (`swings`, negative is
                // left), or `times` swings of `distance` without a list.
                readonly property var swings: {
                    if (wrongMotion.swings !== undefined) {
                        return wrongMotion.swings
                    }
                    const list = []
                    for (let time = 0; time < (wrongMotion.times ?? 3); time++) {
                        list.push(-(wrongMotion.distance ?? 10), wrongMotion.distance ?? 10)
                    }
                    return list
                }
                property int swingNumber: -1
                function swingOn() {
                    swingNumber += 1
                    swing.to = swingNumber < swings.length ? swings[swingNumber] : 0
                    swing.start()
                }
                NumberAnimation {
                    id: swing
                    target: box
                    property: "shakeOffset"
                    duration: box.wrongMotion.swing_ms ?? 60
                    easing.type: Easing.InOutSine
                    onFinished: {
                        if (box.swingNumber < box.swings.length) {
                            box.swingOn()
                        }
                    }
                }
                Connections {
                    target: lockScreen
                    function onWrongTriesChanged() {
                        if (box.wrongMotion.kind === "shake") {
                            swing.stop()
                            box.swingNumber = -1
                            box.swingOn()
                        }
                        password.forceActiveFocus()
                    }
                }
            }

            Component.onCompleted: password.forceActiveFocus()
        }
    }
}
