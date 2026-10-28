// Status item: the battery's charge. Hidden without a battery; the config
// can show the percentage as text (status.battery.show_percentage). Warns
// when the battery runs low, and its menu leads to the Energy settings.

import QtQuick
import Quickshell
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

    // As the config says, until the menu changes it; the change is kept in
    // the user's choices ([status.battery] there holds only this).
    readonly property var kept: panel.choices?.value("status", "battery") ?? wishes
    readonly property bool showsPercentage: kept.show_percentage ?? false
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
    readonly property bool hasSettings: (wishes.settings_command ?? []).length > 0
    menuItems: {
        const items = [
            line(0, stateLine),
            line(1, sourceLine),
            { id: 2, label: "", enabled: false, separator: true,
              checkable: false, checked: false, shortcut: [], children: [] },
            { id: 3, label: wishes.show_percentage_label ?? "Show percentage", enabled: true,
              separator: false, checkable: true, checked: showsPercentage, shortcut: [], children: [] }
        ]
        if (hasSettings) {
            items.push({ id: 4, label: wishes.settings_label ?? "Open Energy Preferences…", enabled: true,
                         separator: false, checkable: false, checked: false, shortcut: [], children: [] })
        }
        return items
    }
    onChosen: id => {
        if (id === 3) {
            panel.choices?.set("status", "battery", { show_percentage: !showsPercentage })
        } else if (id === 4) {
            Quickshell.execDetached(wishes.settings_command)
        }
    }

    // A warning when the charge falls to [status.battery] low_percent and
    // again at critical_percent, while not plugged in; each once per discharge.
    readonly property int lowPercent: wishes.low_percent ?? 10
    readonly property int criticalPercent: wishes.critical_percent ?? 5
    property int warnedAt: 101
    onPercentChanged: {
        if (!present || !UPower.onBattery) {
            warnedAt = 101
            return
        }
        const level = percent <= criticalPercent ? criticalPercent : (percent <= lowPercent ? lowPercent : 101)
        if (level < warnedAt) {
            warnedAt = level
            const critical = level === criticalPercent
            panel.confirmDialog?.ask({
                title: critical ? (wishes.critical_title ?? "Your battery is almost empty.")
                                : (wishes.low_title ?? "Your battery is running low."),
                text: (critical ? (wishes.critical_text ?? "%1% left. The computer will sleep soon; plug it in now to keep working.")
                                : (wishes.low_text ?? "%1% left. Plug the computer in, or save your work."))
                          .replace("%1", Math.round(percent)),
                icon: critical ? "battery-caution" : "battery-low",
                button: wishes.ok_label ?? "OK",
                cancel: ""
            })
        }
    }
}
