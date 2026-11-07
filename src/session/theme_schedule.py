"""[theme_schedule]: one desktop by day and another at night, by the clock.

    [theme_schedule]
    enabled = true
    day = "sea-lion"
    night = "sea-lion-dark"
    night_from = "19:00"
    day_from = "07:00"

pick_desktop.py starts the one for the time of login; the shell
(ThemeSchedule.qml) switches when the time comes, with a warning first.
The shell has the same rule in JavaScript; keep the two alike.
"""

MINUTES_A_DAY = 24 * 60


def minutes_of(text):
    """"19:30" -> 1170; None if it is not a time of day."""
    try:
        hours, minutes = (int(part) for part in str(text).split(":"))
    except ValueError:
        return None
    if not (0 <= hours < 24 and 0 <= minutes < 60):
        return None
    return hours * 60 + minutes


def is_night(schedule, minutes):
    """Whether `minutes` past midnight fall in the schedule's night, which may
    run past midnight."""
    night_from = minutes_of(schedule.get("night_from", "19:00"))
    day_from = minutes_of(schedule.get("day_from", "07:00"))
    if night_from is None or day_from is None or night_from == day_from:
        return False
    if night_from < day_from:
        return night_from <= minutes < day_from
    return minutes >= night_from or minutes < day_from


def wanted_desktop(schedule, minutes):
    """The desktop the schedule wants at `minutes` past midnight; None when it
    is off or names no two desktops."""
    if not isinstance(schedule, dict) or schedule.get("enabled") is not True:
        return None
    day, night = schedule.get("day"), schedule.get("night")
    if not isinstance(day, str) or not isinstance(night, str) or not day or not night:
        return None
    return night if is_night(schedule, minutes % MINUTES_A_DAY) else day
