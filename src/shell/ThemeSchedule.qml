// [theme_schedule]: one desktop by day and another at night, by the clock.
// A banner says so warn_minutes ahead; a click on it switches at once. At
// the time, mavernix-switch makes the change, but not while a window is full
// screen (a game, a video): then it waits until none is.
//
// It acts only when the clock passes night_from or day_from, so a theme the
// user picks in between stays until the next one. The login starts the right
// one to begin with (src/session/theme_schedule.py, the same rule).

import QtQuick
import Quickshell
import Quickshell.Wayland

Scope {
    id: schedule

    required property var settings
    required property var choices
    property var banners: null

    function wish(key, fallback) {
        return choices.value("theme_schedule", key) ?? fallback
    }
    readonly property var words: (settings.config.theme_schedule ?? ({})).words ?? ({})

    readonly property bool enabled: wish("enabled", false) === true
    readonly property string day: wish("day", "")
    readonly property string night: wish("night", "")
    readonly property int nightFrom: minutesOf(wish("night_from", "19:00"))
    readonly property int dayFrom: minutesOf(wish("day_from", "07:00"))
    readonly property int warnMinutes: Math.max(0, wish("warn_minutes", 5))
    readonly property bool works: enabled && day !== "" && night !== "" && day !== night
        && nightFrom >= 0 && dayFrom >= 0 && nightFrom !== dayFrom

    // The desktop running now; a switch restarts the shell with the new one.
    readonly property string running: settings.desktop ?? ""

    // "19:30" -> 1170; -1 if it is not a time of day.
    function minutesOf(text) {
        const parts = /^(\d{1,2}):(\d{2})$/.exec(String(text))
        if (parts === null || Number(parts[1]) > 23 || Number(parts[2]) > 59) {
            return -1
        }
        return Number(parts[1]) * 60 + Number(parts[2])
    }
    function isNight(minutes) {
        return nightFrom < dayFrom ? minutes >= nightFrom && minutes < dayFrom
                                   : minutes >= nightFrom || minutes < dayFrom
    }
    function wantedAt(minutes) {
        return isNight(((minutes % 1440) + 1440) % 1440) ? night : day
    }
    function minutesNow(now) {
        return now.getHours() * 60 + now.getMinutes()
    }
    // "sea-lion-dark" -> "Sea Lion Dark", as Settings shows it.
    function readable(name) {
        return name.split("-").filter(word => word !== "")
            .map(word => word[0].toUpperCase() + word.slice(1)).join(" ")
    }
    function timeOf(minutes) {
        return String(Math.floor(minutes / 60)).padStart(2, "0") + ":" + String(minutes % 60).padStart(2, "0")
    }

    // What the clock wanted at the last look; empty before the first.
    property string lastWanted: ""
    // The switch to come: { desktop, at } (ms since 1970), or null.
    property var upcoming: null

    readonly property bool fullScreenWindow: ToplevelManager.toplevels.values.some(window => window.fullscreen)

    function look() {
        if (!works) {
            lastWanted = ""
            upcoming = null
            return
        }
        const now = new Date()
        const minutes = minutesNow(now)
        const wanted = wantedAt(minutes)

        // Ahead of a change: the banner, once.
        if (upcoming === null && warnMinutes > 0) {
            const soon = wantedAt(minutes + warnMinutes)
            if (soon !== wanted && soon !== running) {
                const change = isNight(minutes) ? dayFrom : nightFrom
                const at = new Date(now)
                at.setHours(Math.floor(change / 60), change % 60, 0, 0)
                if (at < now) {
                    at.setDate(at.getDate() + 1)
                }
                announce(soon, at.getTime(), change)
            }
        }
        // The change itself, also when the warning was missed (asleep then).
        if (lastWanted !== "" && wanted !== lastWanted && wanted !== running
                && (upcoming === null || upcoming.desktop !== wanted)) {
            if (warnMinutes > 0) {
                announce(wanted, now.getTime() + warnMinutes * 60000, minutes + warnMinutes)
            } else {
                upcoming = { desktop: wanted, at: now.getTime() }
            }
        }
        lastWanted = wanted

        if (upcoming !== null && now.getTime() >= upcoming.at && !fullScreenWindow) {
            switchTo(upcoming.desktop)
        }
    }

    function announce(desktop, at, minutes) {
        upcoming = { desktop: desktop, at: at }
        if (banners === null) {
            return
        }
        const notice = noticeKind.createObject(schedule, {
            appName: words.app_name ?? "Settings",
            appIcon: "preferences-system",
            summary: (words.soon_title ?? "%1 at %2").replace("%1", readable(desktop)).replace("%2", timeOf(minutes % 1440)),
            body: words.soon_text ?? "Click to change it now. Open windows keep their look until reopened.",
            pressed: () => schedule.switchTo(desktop)
        })
        notice.id = -Math.floor(at / 1000)
        banners.showOwn(notice)
    }

    function switchTo(desktop) {
        upcoming = null
        if (desktop !== running) {
            Quickshell.execDetached(["mavernix-switch", desktop])
        }
    }

    Component {
        id: noticeKind
        ShellNotice {}
    }

    // Often enough to switch within half a minute of the time.
    Timer {
        interval: 15000
        running: schedule.works
        repeat: true
        triggeredOnStart: true
        onTriggered: schedule.look()
    }
    // Changed in Settings: what was announced may no longer hold.
    readonly property string plan: [works, day, night, nightFrom, dayFrom, warnMinutes].join(" ")
    onPlanChanged: {
        lastWanted = ""
        upcoming = null
        look()
    }
}
