"""Says which desktop starts when start-desktop is given no name.

    python pick_desktop.py

Prints the user's choice if it can start, else the desktop the Theme
Installer added last, else plain.
"""

import sys

from themes import theme_available
from user_choices import default_desktop_path, find_desktop_config, load_desktop_config, user_config_dir

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


def main():
    for name in [chosen_desktop()] + added_desktops():
        if name is not None and can_start(name):
            print(name)
            return
    print(FALLBACK_DESKTOP)


if __name__ == "__main__":
    sys.exit(main())
