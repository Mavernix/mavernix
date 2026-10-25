// Status item: Bluetooth. Its menu switches Bluetooth, lists known devices
// with connect/disconnect submenus, and while open, looks for devices
// nearby and pairs the one chosen.

import QtQuick
import Quickshell
import Quickshell.Bluetooth
import ".."

StatusItem {
    id: item

    readonly property var adapter: Bluetooth.defaultAdapter
    readonly property bool on: adapter !== null && adapter.enabled
    readonly property var allDevices: adapter !== null ? adapter.devices.values : []

    // Nameless devices are reported under their address; they mean nothing to
    // a person.
    function hasName(device) {
        return device.name !== "" && device.name.replace(/-/g, ":") !== device.address
    }
    readonly property var devices: allDevices.filter(device => device.paired || device.connected)
    readonly property var nearby: allDevices.filter(device => !device.paired && !device.connected && hasName(device))

    shown: adapter !== null
    iconName: on ? "bluetooth-active" : "bluetooth-disabled"

    // Wording: [status.bluetooth].
    readonly property var words: (panel.settings.config.status ?? ({})).bluetooth ?? ({})

    // Discovery runs only while the menu is open, since it costs power and
    // keeps the adapter busy; discovery started by someone else is left alone.
    property bool startedLooking: false
    onOpenChanged: {
        if (adapter === null) {
            return
        }
        if (open && on && !adapter.discovering) {
            adapter.discovering = true
            startedLooking = true
        } else if (!open && startedLooking) {
            adapter.discovering = false
            startedLooking = false
        }
    }

    // Once paired, the chosen device is trusted (so it may reconnect by
    // itself) and connected.
    property var pairingDevice: null
    Connections {
        target: item.pairingDevice
        function onPairedChanged() {
            const device = item.pairingDevice
            if (device !== null && device.paired) {
                device.trusted = true
                device.connect()
                item.pairingDevice = null
            }
        }
    }

    // Entry ids: fixed ones, and per-device ones counted from a base.
    readonly property int switchId: 1
    readonly property int settingsId: 2
    readonly property int deviceBaseId: 1000
    readonly property int deviceActionBaseId: 2000
    readonly property int nearbyBaseId: 3000

    function entry(id, label, enabled, children) {
        return { id: id, label: label, enabled: enabled, separator: false,
                 checkable: false, checked: false, shortcut: [], children: children ?? [] }
    }
    readonly property var separator: ({ id: 0, label: "", enabled: false, separator: true,
                                        checkable: false, checked: false, shortcut: [], children: [] })

    menuItems: {
        const items = [
            entry(0, on ? (words.on_label ?? "Bluetooth is on") : (words.off_label ?? "Bluetooth is off"), false),
            entry(switchId, on ? (words.turn_off_label ?? "Turn off") : (words.turn_on_label ?? "Turn on"), true)
        ]
        if (on) {
            if (devices.length > 0) {
                items.push(separator)
                items.push(entry(0, words.devices_label ?? "Devices", false))
                for (let index = 0; index < devices.length; index++) {
                    const device = devices[index]
                    const inside = []
                    if (device.batteryAvailable) {
                        inside.push(entry(0, (words.battery_label ?? "Battery: %1%").replace("%1", Math.round(device.battery * 100)), false))
                    }
                    inside.push(entry(deviceActionBaseId + index,
                                      device.connected ? (words.disconnect_label ?? "Disconnect")
                                                       : (words.connect_label ?? "Connect"), true))
                    const deviceEntry = entry(deviceBaseId + index, device.name, true, inside)
                    deviceEntry.checkable = true
                    deviceEntry.checked = device.connected
                    items.push(deviceEntry)
                }
            }

            items.push(separator)
            items.push(entry(0, words.nearby_label ?? "Nearby", false))
            if (nearby.length === 0) {
                items.push(entry(0, words.searching_label ?? "Searching…", false))
            }
            for (let index = 0; index < nearby.length; index++) {
                const pairing = nearby[index] === pairingDevice || nearby[index].pairing
                items.push(entry(nearbyBaseId + index,
                                 pairing ? (words.pairing_label ?? "%1 (pairing…)").replace("%1", nearby[index].name)
                                         : nearby[index].name,
                                 !pairing))
            }
        }
        if ((words.settings_command ?? []).length > 0) {
            items.push(separator)
            items.push(entry(settingsId, words.settings_label ?? "Bluetooth Settings…", true))
        }
        return items
    }

    onChosen: id => {
        if (adapter === null) {
            return
        }
        if (id === switchId) {
            adapter.enabled = !adapter.enabled
        } else if (id === settingsId) {
            Quickshell.execDetached(words.settings_command)
        } else if (id >= nearbyBaseId) {
            const device = nearby[id - nearbyBaseId]
            if (device !== undefined) {
                pairingDevice = device
                device.pair()
            }
        } else if (id >= deviceActionBaseId) {
            const device = devices[id - deviceActionBaseId]
            if (device === undefined) {
                return
            }
            if (device.connected) {
                device.disconnect()
            } else {
                device.connect()
            }
        }
    }
}
