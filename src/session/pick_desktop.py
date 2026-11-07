"""Says which desktop starts when start-desktop is given no name.

    python pick_desktop.py

Prints the user's choice if it can start, else the desktop the Theme
Installer added last, else plain. With [theme_schedule] on in that desktop's
choices, the desktop for the time of day takes its place (theme_schedule.py).
"""

import datetime
import sys

from themes import theme_available
from theme_schedule import wanted_desktop
from user_choices import (default_desktop_path, find_desktop_config, load_desktop_config, user_config_dir,
                          with_user_choices)

FALLBACK_DESKTOP = "plain"


def can_start(name):
    """Whether the desktop `name` has a config whose theme is there."""
    config_path = find_desktop_config(name)
    if config_path is None:
        return False
    theme = load_desktop_config(config_path).get("theme")
    return theme is not None and theme_available(theme)


def chosen_desktop():
    """The desktop the user chose, or None."""
    try:
        return default_desktop_path().read_text().strip() or None
    except FileNotFoundError:
        return None


def added_desktops():
    """The desktops the Theme Installer added, the latest first."""
    configs = sorted(user_config_dir().glob("*.toml"), key=lambda path: path.stat().st_mtime_ns, reverse=True)
    return [path.stem for path in configs]


def for_time_of_day(name, now=None):
    """The desktop [theme_schedule] wants now, if `name`'s choices turn it on
    and that desktop can start; else `name`."""
    config_path = find_desktop_config(name)
    config = with_user_choices(load_desktop_config(config_path), config_path)
    now = now or datetime.datetime.now()
    wanted = wanted_desktop(config.get("theme_schedule"), now.hour * 60 + now.minute)
    return wanted if wanted is not None and can_start(wanted) else name


def main():
    for name in [chosen_desktop()] + added_desktops():
        if name is not None and can_start(name):
            print(for_time_of_day(name))
            return
    print(FALLBACK_DESKTOP)


if __name__ == "__main__":
    sys.exit(main())
