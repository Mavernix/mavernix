// The desktop shell: everything on the desktop that is not a window. Run by
// Quickshell; DESKTOP_SHELL_SETTINGS names the JSON settings file written
// by src/session/make_shell_config.py.

import QtQuick
import Quickshell
import Quickshell.Io
import Shell.Native

ShellRoot {
    id: shell

    FileView {
        id: settingsFile
        path: Quickshell.env("DESKTOP_SHELL_SETTINGS")
        // Every component needs the settings from its first moment.
        blockLoading: true
    }

    readonly property var settings: JSON.parse(settingsFile.text())


    // The screen for the panel, dock and overlays: [display] main_screen, else
    // the first one.
    readonly property var mainScreen: {
        const wanted = (settings.config.display ?? ({})).main_screen
        const screens = Quickshell.screens
        return screens.find(screen => screen.name === wanted) ?? screens[0] ?? null
    }

    Variants {
        model: Quickshell.screens
        DesktopBackground {
            required property var modelData
            screen: modelData
            settings: shell.settings
            fill: shell.desktopFill
            onPictureFailed: shell.unreadablePicture = shell.chosenPicture
            showsFiles: modelData === shell.mainScreen
            // (Not named `userChoices` there, which would shadow this id.)
            choices: userChoices
        }
    }

    // [desktop] picture: a full path, or a path inside the theme; empty for the
    // theme's background.
    readonly property string chosenPicture: userChoices.value("desktop", "picture") ?? ""
    // A chosen picture that failed to load; the theme's background is shown.
    property string unreadablePicture: ""
    readonly property var desktopFill: {
        if (chosenPicture === "" || chosenPicture === unreadablePicture) {
            return settings.theme.desktop.background
        }
        return { image: chosenPicture, fit: "cover" }
    }

    // Global menus. The registrar's name must be owned before applications
    // start, or they keep their menu bars in their windows.
    MenuRegistrar {}

    ActiveWindow {
        id: activeWindow
    }

    ApplicationMenu {
        id: applicationMenu
        service: activeWindow.menuService
        path: activeWindow.menuPath
    }

    ConfirmDialog {
        screen: shell.mainScreen
        id: confirmDialog
        settings: shell.settings
        onRequestChanged: {
            if (request !== null) {
                sounds.play("dialog-question")
            }
        }
    }

    Banners {
        id: banners
        screen: shell.mainScreen
        settings: shell.settings
        sounds: sounds
    }

    readonly property bool searchOpen: searchBox.open

    WindowOverview {
        id: windowOverview
        screen: shell.mainScreen
        settings: shell.settings
        desktopFill: shell.desktopFill
        userChoices: userChoices
    }

    ProgramSwitcher {
        id: programSwitcher
        screen: shell.mainScreen
        settings: shell.settings
        choices: userChoices
        onMinimizedChosen: window => dock.bringBack(window)
    }

    Search {
        id: searchBox
        screen: shell.mainScreen
        settings: shell.settings
        choices: userChoices
    }

    NotificationList {
        id: notificationList
        screen: shell.mainScreen
        settings: shell.settings
        banners: banners
    }

    UserChoices {
        id: userChoices
        settings: shell.settings
    }

    Extensions {
        id: extensions
        settings: shell.settings
        userChoices: userChoices
        shell: shell
        screen: shell.mainScreen
    }

    Sounds {
        id: sounds
        settings: shell.settings
        choices: userChoices
    }

    OutputVolume {
        id: outputVolume
        sounds: sounds
        playsFeedback: userChoices.value("sounds", "volume_feedback") ?? true
        onChangedHere: volumeDisplay.show()
    }

    VolumeDisplay {
        id: volumeDisplay
        screen: shell.mainScreen
        settings: shell.settings
        outputVolume: outputVolume
    }

    Panel {
        screen: shell.mainScreen
        settings: shell.settings
        applicationMenu: applicationMenu
        confirmDialog: confirmDialog
        outputVolume: outputVolume
        shell: shell
        extensions: extensions
        // Out of sight while a full-screen overlay covers the screen.
        concealed: appGrid.open || windowOverview.showing
    }

    AppGrid {
        screen: shell.mainScreen
        id: appGrid
        settings: shell.settings
        choices: userChoices
        desktopFill: shell.desktopFill
        onDragMoved: (entry, iconSource, screenX, screenY) =>
            dock.dragFromOutside(entry.id, iconSource, screenX, screenY)
        onDragEnded: dock.endDrag()
        onDragGivenUp: dock.giveUpDrag()
    }

    LockScreen {
        id: lockScreen
        settings: shell.settings
        desktopFill: shell.desktopFill
    }

    Idle {
        choices: userChoices
        lockScreen: lockScreen
    }

    Authentication {
        settings: shell.settings
        dialog: confirmDialog
    }

    // [sounds] volume_step, a share of the full loudness.
    readonly property real volumeStep: (settings.config.sounds ?? ({})).volume_step ?? 0.05

    function run(action) {
        if (action === "app_grid") {
            appGrid.toggle()
        } else if (action === "volume_up") {
            outputVolume.changeBy(volumeStep)
        } else if (action === "volume_down") {
            outputVolume.changeBy(-volumeStep)
        } else if (action === "volume_mute") {
            outputVolume.toggleMute()
        } else if (action === "lock") {
            lockScreen.lock()
        } else if (action === "notification_list") {
            notificationList.toggle()
        } else if (action === "search") {
            searchBox.toggle()
        } else if (action === "window_overview") {
            windowOverview.toggle()
        } else if (action === "switch_program") {
            programSwitcher.step(1)
        } else if (action === "switch_program_back") {
            programSwitcher.step(-1)
        } else if (action === "empty_trash") {
            dock.emptyTrash()
        } else {
            console.warn("shell: the config asks for an action the shell does not have:", action,
                         "- known: app_grid, volume_up, volume_down, volume_mute, lock, notification_list,",
                         "search, window_overview, switch_program, switch_program_back, empty_trash")
        }
    }

    // The compositor's key bindings run `qs ipc call shell run ACTION`.
    IpcHandler {
        target: "shell"
        function run(action: string): void {
            shell.run(action)
        }
    }

    // `mavernix-sound EVENT` runs `qs ipc call sounds play EVENT`.
    IpcHandler {
        target: "sounds"
        function play(event: string): void {
            sounds.play(event)
            // Screenshots may also flash the screen ([screenshot] flash).
            if (event === "screen-capture" && (userChoices.value("screenshot", "flash") ?? false)) {
                screenFlash.flash()
            }
        }
    }

    ScreenFlash {
        id: screenFlash
        screen: shell.mainScreen
        settings: shell.settings
    }

    Component.onCompleted: {
        // Off unless [session] reload_shell_on_change: an update changes these
        // files under a running desktop, which should keep going until next login.
        Quickshell.watchFiles = (settings.config.session ?? ({})).reload_shell_on_change ?? false

        sounds.play("desktop-login")
    }

    Dock {
        id: dock
        screen: shell.mainScreen
        settings: shell.settings
        shell: shell
        choices: userChoices
        sounds: sounds
        confirmDialog: confirmDialog
        // In front of overlays that reach down behind it.
        raised: appGrid.open || notificationList.visible || windowOverview.showing
    }
}
