// Status item: the network connection, preferring a cable over wireless.
//
// Its menu is the Wi-Fi menu: whether Wi-Fi is on, the networks in reach
// (a tick on the one in use, a lock on those with a password, the signal's
// strength), joining one, asking for its password when needed, and the
// network settings. Wording: [status.network].
//
// [status.network] tray_program names a network applet whose icon the tray
// leaves out, so there is one network icon; the applet still runs, as
// NetworkManager's password helper. With `menu = "program"` this item shows
// that program's menu instead of its own.

import QtQuick
import Quickshell
import Quickshell.Networking
import Quickshell.Services.SystemTray
import ".."

StatusItem {
    id: item

    readonly property var devices: Networking.backend !== NetworkBackendType.None
                                   ? Networking.devices.values : []

    function firstDevice(type, mustBeConnected) {
        for (const device of devices) {
            if (device.type === type && (device.connected || !mustBeConnected)) {
                return device
            }
        }
        return null
    }
    readonly property var cable: firstDevice(DeviceType.Wired, true)
    readonly property var wireless: firstDevice(DeviceType.Wifi, false)

    readonly property var wirelessNetwork: {
        if (wireless === null) {
            return null
        }
        for (const network of wireless.networks.values) {
            if (network.connected) {
                return network
            }
        }
        return null
    }

    shown: devices.length > 0

    // Wording: [status.network].
    readonly property var words: (panel.settings.config.status ?? ({})).network ?? ({})

    // The tray program whose menu replaces ours, or null.
    readonly property string trayProgramName: words.tray_program ?? ""
    readonly property var trayProgram: {
        if (trayProgramName === "" || (words.menu ?? "own") !== "program") {
            return null
        }
        return SystemTray.items.values.find(program => program.id === item.trayProgramName) ?? null
    }
    TrayMenu {
        id: programMenu
        menu: item.trayProgram !== null && item.trayProgram.hasMenu ? item.trayProgram.menu : null
    }

    // The networks look for others only while the menu is open, as scanning
    // costs power; a scan started by someone else is left alone.
    property bool startedScanning: false
    onOpenChanged: {
        if (wireless === null || trayProgram !== null) {
            return
        }
        if (open && Networking.wifiEnabled && !wireless.scannerEnabled) {
            wireless.scannerEnabled = true
            startedScanning = true
        } else if (!open && startedScanning) {
            wireless.scannerEnabled = false
            startedScanning = false
        }
    }

    // In reach: the one in use, then the strongest first; a network's
    // several access points are one.
    readonly property var networks: {
        if (wireless === null || !Networking.wifiEnabled) {
            return []
        }
        const named = wireless.networks.values.filter(network => network.name !== "")
        return named.sort((one, other) => (other.connected - one.connected)
                                          || (other.signalStrength - one.signalStrength))
    }

    function signalIcon(strength) {
        if (strength < 0.25) {
            return "network-wireless-signal-weak"
        }
        if (strength < 0.5) {
            return "network-wireless-signal-ok"
        }
        if (strength < 0.75) {
            return "network-wireless-signal-good"
        }
        return "network-wireless-signal-excellent"
    }

    function isOpen(network) {
        return network.security === WifiSecurityType.Open || network.security === WifiSecurityType.Owe
    }
    // Networks that log in with a user name and a certificate are set up in
    // the network settings, not from a menu.
    function needsSetUp(network) {
        return [WifiSecurityType.Wpa2Eap, WifiSecurityType.WpaEap, WifiSecurityType.Wpa3SuiteB192,
                WifiSecurityType.DynamicWep, WifiSecurityType.Leap].includes(network.security)
    }

    // Entry ids: fixed ones, and one per network counted from a base.
    readonly property int switchId: 1
    readonly property int settingsId: 2
    readonly property int otherId: 3
    readonly property int disconnectId: 4
    readonly property int networkBaseId: 1000

    function entry(id, label, enabled) {
        return { id: id, label: label, enabled: enabled, separator: false,
                 checkable: false, checked: false, shortcut: [], children: [] }
    }
    readonly property var separator: ({ id: 0, label: "", enabled: false, separator: true,
                                        checkable: false, checked: false, shortcut: [], children: [] })
    readonly property bool hasSettings: (words.settings_command ?? []).length > 0

    readonly property var ownMenu: {
        const items = []
        if (cable !== null) {
            items.push(entry(0, (words.cable_label ?? "Connected by cable: %1").replace("%1", cable.name), false))
        }
        if (wireless !== null) {
            if (items.length > 0) {
                items.push(separator)
            }
            const on = Networking.wifiEnabled
            items.push(entry(0, on ? (words.on_label ?? "Wi-Fi: On") : (words.off_label ?? "Wi-Fi: Off"), false))
            items.push(entry(switchId, on ? (words.turn_off_label ?? "Turn Wi-Fi Off")
                                          : (words.turn_on_label ?? "Turn Wi-Fi On"), true))
            if (on && wirelessNetwork !== null) {
                items.push(entry(disconnectId, (words.disconnect_label ?? "Disconnect from “%1”")
                                                   .replace("%1", wirelessNetwork.name), true))
            }
            if (on) {
                items.push(separator)
                if (networks.length === 0) {
                    items.push(entry(0, words.searching_label ?? "Looking for networks…", false))
                }
                for (let index = 0; index < networks.length; index++) {
                    const network = networks[index]
                    const row = entry(networkBaseId + index, network.name, !network.stateChanging)
                    row.checkable = true
                    row.checked = network.connected
                    const lock = isOpen(network) ? [] : [picture(words.lock_icon ?? "network-wireless-encrypted")]
                    row.pictures = lock.concat([picture(signalIcon(network.signalStrength))])
                        .filter(address => address !== "")
                    items.push(row)
                }
                if (hasSettings) {
                    items.push(separator)
                    items.push(entry(otherId, words.join_other_label ?? "Join Other Network…", true))
                }
            }
        }
        if (hasSettings) {
            items.push(separator)
            items.push(entry(settingsId, words.settings_label ?? "Open Network Preferences…", true))
        }
        return items
    }

    menuItems: trayProgram !== null ? programMenu.items : ownMenu
    onChosen: id => {
        if (trayProgram !== null) {
            programMenu.choose(id)
        } else if (id === switchId) {
            Networking.wifiEnabled = !Networking.wifiEnabled
        } else if (id === disconnectId) {
            wirelessNetwork?.disconnect()
        } else if (id === settingsId) {
            Quickshell.execDetached(words.settings_command)
        } else if (id === otherId) {
            // Straight to the settings' dialog for a hidden network.
            Quickshell.execDetached(words.settings_command.concat(["join-hidden"]))
        } else if (id >= networkBaseId) {
            join(networks[id - networkBaseId])
        }
    }

    // A saved or open network is joined at once; a new one with a password
    // asks for it first. If the network turns the password down, it is asked
    // for again.
    property var joining: null
    function join(network) {
        if (network === undefined || network.connected) {
            return
        }
        if (needsSetUp(network)) {
            tell((words.enterprise_label ?? "“%1” signs in with a user name or a certificate. Set it up with NetworkManager's connection editor (nm-connection-editor).")
                     .replace("%1", network.name))
            return
        }
        joining = network
        if (network.known || isOpen(network)) {
            network.connect()
        } else {
            askPassword(network, false)
        }
    }

    // A message with only "OK".
    function tell(text) {
        panel.confirmDialog?.ask({ title: text, text: "", icon: words.password_icon ?? "network-wireless",
                                   button: words.ok_label ?? "OK", cancel: "" })
    }

    function askPassword(network, again) {
        const dialog = panel.confirmDialog
        if (dialog === null) {
            joining = null
            return
        }
        const question = again ? (words.wrong_password_label ?? "The password for “%1” was not accepted. Try again.")
                               : (words.password_label ?? "The Wi-Fi network “%1” requires a password.")
        dialog.ask({
            title: question.replace("%1", network.name),
            text: "",
            icon: words.password_icon ?? "network-wireless",
            button: words.join_label ?? "Join",
            cancel: words.cancel_label ?? "Cancel",
            field: { placeholder: words.password_placeholder ?? "Password", secret: true },
            agreed: typed => network.connectWithPsk(typed),
            cancelled: () => { item.joining = null }
        })
    }

    Connections {
        target: item.joining
        function onConnectedChanged() {
            if (item.joining !== null && item.joining.connected) {
                item.joining = null
            }
        }
        function onConnectionFailed(reason) {
            const network = item.joining
            if (network !== null && reason === ConnectionFailReason.NoSecrets && !item.isOpen(network)) {
                item.askPassword(network, true)
            } else {
                item.joining = null
                if (network !== null) {
                    item.tell((item.words.failed_label ?? "Could not join “%1”. Move closer to it, or try again in a moment.")
                                  .replace("%1", network.name))
                }
            }
        }
    }

    iconName: {
        if (cable !== null) {
            return "network-wired"
        }
        if (wireless === null) {
            return "network-offline"
        }
        if (!Networking.wifiEnabled) {
            return "network-wireless-offline"
        }
        if (wirelessNetwork === null) {
            return "network-wireless-signal-none"
        }
        return signalIcon(wirelessNetwork.signalStrength)
    }
}
