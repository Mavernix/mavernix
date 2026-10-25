// Status item: the network connection, preferring a cable over wireless.
//
// Its own menu only names the connection. If [status.network] tray_program
// names a running network applet, this item shows that program's menu
// instead and the tray leaves its icon out, so there is one network icon.

import QtQuick
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

    // Names the wireless network or the cable's device; nothing connected, no menu.
    readonly property string connectionName: {
        if (cable !== null) {
            return cable.name
        }
        if (wirelessNetwork !== null) {
            return wirelessNetwork.name
        }
        return ""
    }
    readonly property var ownMenu: connectionName === "" ? [] : [{
        id: 0, label: connectionName, enabled: false, separator: false,
        checkable: false, checked: false, shortcut: [], children: []
    }]

    // The tray program whose menu replaces ours, or null.
    readonly property string trayProgramName:
        ((panel.settings.config.status ?? ({})).network ?? ({})).tray_program ?? ""
    readonly property var trayProgram: {
        if (trayProgramName === "") {
            return null
        }
        return SystemTray.items.values.find(program => program.id === item.trayProgramName) ?? null
    }
    TrayMenu {
        id: programMenu
        menu: item.trayProgram !== null && item.trayProgram.hasMenu ? item.trayProgram.menu : null
    }

    menuItems: trayProgram !== null ? programMenu.items : ownMenu
    onChosen: id => {
        if (trayProgram !== null) {
            programMenu.choose(id)
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
        const strength = wirelessNetwork.signalStrength
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
}
