// Locks the screen after a time without input ([idle] lock_after_seconds),
// before the machine sleeps, and when logind asks; unlocks when logind asks.
// Switching the screens off is left to the compositor.

import QtQml
import QtQuick
import Quickshell.Wayland
import Shell.Native

Item {
    id: idle

    // The config's [idle] table is read through `choices`.
    required property var choices
    required property var lockScreen

    // 0 or less: never.
    readonly property real lockAfterSeconds: choices.value("idle", "lock_after_seconds") ?? 0
    readonly property bool lockBeforeSleep: choices.value("idle", "lock_before_sleep") ?? true

    // Remade whenever the time changes: an IdleMonitor whose timeout was changed,
    // or that was disabled and enabled again, stops firing.
    Instantiator {
        model: idle.lockAfterSeconds > 0 ? [idle.lockAfterSeconds] : []

        IdleMonitor {
            required property real modelData
            timeout: modelData
            // Video players and calls that inhibit idle also keep it from locking.
            respectInhibitors: true
            onIsIdleChanged: {
                if (isIdle) {
                    idle.lockScreen.lock()
                }
            }
        }
    }

    SessionWatch {
        id: session
        onAboutToSleep: {
            if (idle.lockBeforeSleep) {
                idle.lockScreen.lock()
            } else {
                readyToSleep()
            }
        }
        onLockRequested: idle.lockScreen.lock()
        onUnlockRequested: idle.lockScreen.unlock()
    }

    // Sleep is held back only until the lock screen is really up.
    Connections {
        target: idle.lockScreen
        function onSecureChanged() {
            if (idle.lockScreen.secure) {
                session.readyToSleep()
            }
        }
    }
}
