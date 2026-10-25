// Status item: the battery's charge. Hidden without a battery; the config
// can show the percentage as text (status.battery.show_percentage).

import QtQuick
import Quickshell.Services.UPower
import ".."

StatusItem {
    id: item

    readonly property var battery: UPower.displayDevice
    readonly property bool present: battery !== null && battery.ready && battery.isLaptopBattery
    readonly property real percent: present ? battery.percentage * 100 : 0

    shown: present

    iconName: {
        if (!present) {
            return "battery-missing"
        }
        if (battery.state === UPowerDeviceState.FullyCharged) {
            return "battery-full-charged"
        }
        if (battery.state === UPowerDeviceState.Charging
                || battery.state === UPowerDeviceState.PendingCharge) {
            return "battery-good-charging"
        }
        // Icons exist for every tenth: "battery-level-0" to "battery-level-100".
        return "battery-level-" + Math.round(percent / 10) * 10
    }

    // Settings and wording: [status.battery].
    readonly property var wishes: (panel.settings.config.status ?? ({})).battery ?? ({})

    // Starts as the config says; the menu toggles it until the next restart
    // (the shell never writes the config file).
    property bool showsPercentage: wishes.show_percentage ?? false
    text: showsPercentage ? Math.round(percent) + "%" : ""

    readonly property bool charged: present && battery.state === UPowerDeviceState.FullyCharged
    readonly property bool charging: present && (battery.state === UPowerDeviceState.Charging
                                                 || battery.state === UPowerDeviceState.PendingCharge)

    function hoursAndMinutes(seconds) {
        const minutes = Math.round(seconds / 60)
        const rest = minutes % 60
        return Math.floor(minutes / 60) + ":" + (rest < 10 ? "0" : "") + rest
    }

    readonly property string stateLine: {
        if (!present) {
            return ""
        }
        if (charged) {
            return wishes.charged_label ?? "Charged"
        }
        if (charging) {
            return battery.timeToFull > 0
                ? (wishes.until_full_label ?? "%1 until full").arg(hoursAndMinutes(battery.timeToFull))
                : Math.round(percent) + "%"
        }
        return battery.timeToEmpty > 0
            ? (wishes.remaining_label ?? "%1 remaining").arg(hoursAndMinutes(battery.timeToEmpty))
            : Math.round(percent) + "%"
    }
    readonly property string sourceLine: UPower.onBattery
        ? (wishes.on_battery_label ?? "On battery")
        : (wishes.on_adapter_label ?? "On mains power")

    function line(number, label) {
        return { id: number, label: label, enabled: false, separator: false,
                 checkable: false, checked: false, shortcut: [], children: [] }
    }
    menuItems: [
        line(0, stateLine),
        line(1, sourceLine),
        { id: 2, label: "", enabled: false, separator: true,
          checkable: false, checked: false, shortcut: [], children: [] },
        { id: 3, label: wishes.show_percentage_label ?? "Show percentage", enabled: true,
          separator: false, checkable: true, checked: showsPercentage, shortcut: [], children: [] }
    ]
    onChosen: id => {
        if (id === 3) {
            showsPercentage = !showsPercentage
        }
    }
}
