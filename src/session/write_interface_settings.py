"""Writes the theme's colour scheme, icon theme and font, and the config's
window buttons, where GTK reads them.

    python write_interface_settings.py SHELL-SETTINGS.json [--window-buttons-only]

GTK, the portal and others read these from dconf. start-desktop points
DCONF_PROFILE at a database of this session's own ("mavernix"), so Plasma's
choices do not show here and Plasma's store is not touched.

The button layout must be written: left out, GTK header bars get the
schema's default of a close button only. It comes from [gtk] button_side and
button_order, falling back to [window]. `--window-buttons-only` writes it
alone, for mavernix-reconfigure.

Without `gsettings` it changes nothing.
"""

import json
import shutil
import subprocess
import sys

INTERFACE_SCHEMA = "org.gnome.desktop.interface"
WINDOW_BUTTONS_SCHEMA = "org.gnome.desktop.wm.preferences"

BUTTON_SIDES = ("left", "right")

LIGHT_SCHEME = "prefer-light"
DARK_SCHEME = "prefer-dark"

# dconf's service is started on demand; if it hangs, the desktop must start anyway.
WRITE_TIME_LIMIT_SECONDS = 5

# Theme sizes are pixels; GTK font sizes are points at 96 pixels per inch.
POINTS_PER_PIXEL = 72 / 96


def say(message):
    print(f"write_interface_settings: {message}", file=sys.stderr)


def button_layout(config):
    """GTK's layout string, e.g. "close,minimize,maximize:" (left side) or
    ":minimize,maximize,close" (right). None for an unknown side."""
    gtk = config.get("gtk", {})
    window = config.get("window", {})
    side = gtk.get("button_side", window.get("button_side", "left"))
    buttons = gtk.get("button_order", window.get("button_order", ["close", "minimize", "maximize"]))
    if side not in BUTTON_SIDES:
        say(f'button_side is "{side}"; it must be "left" or "right", GTK programs keep their own layout')
        return None
    listed = ",".join(buttons)
    return f"{listed}:" if side == "left" else f":{listed}"


def wanted_window_button_values(config):
    """The keys of org.gnome.desktop.wm.preferences the config decides."""
    layout = button_layout(config)
    return {} if layout is None else {"button-layout": layout}


def wanted_interface_values(theme):
    """The org.gnome.desktop.interface values the theme decides."""
    values = {}
    # A theme without [controls] leaves applications alone.
    controls = theme.get("controls")
    if controls is not None:
        values["color-scheme"] = DARK_SCHEME if controls.get("dark", False) else LIGHT_SCHEME
    icon_theme = theme.get("icons", {}).get("theme")
    if icon_theme:
        values["icon-theme"] = icon_theme
    font = theme.get("fonts", {}).get("interface")
    if font and font.get("family") and font.get("size"):
        points = round(font["size"] * POINTS_PER_PIXEL, 2)
        values["font-name"] = f"{font['family']} {points:g}"
    return values


def write_values(schema, values):
    """Sets each key; False when dconf times out, so the caller can stop."""
    for key, value in values.items():
        try:
            result = subprocess.run(["gsettings", "set", schema, key, value], capture_output=True,
                                    text=True, timeout=WRITE_TIME_LIMIT_SECONDS)
        except subprocess.TimeoutExpired:
            say(f"setting {key} took longer than {WRITE_TIME_LIMIT_SECONDS} s; is dconf's service "
                "running (ca.desrt.dconf on the session bus)? GTK programs keep the user's own settings")
            return False
        if result.returncode != 0:
            say(f"could not set {key} to {value}: {result.stderr.strip()}")
    return True


def main():
    arguments = sys.argv[1:]
    buttons_only = "--window-buttons-only" in arguments
    paths = [argument for argument in arguments if argument != "--window-buttons-only"]
    if len(paths) != 1:
        sys.exit("usage: write_interface_settings.py SHELL-SETTINGS.json [--window-buttons-only]")
    with open(paths[0]) as file:
        settings = json.load(file)

    if shutil.which("gsettings") is None:
        say("gsettings is not installed (package glib2); GTK programs keep the user's own settings")
        return
    if not buttons_only:
        if not write_values(INTERFACE_SCHEMA, wanted_interface_values(settings["theme"])):
            return
    write_values(WINDOW_BUTTONS_SCHEMA, wanted_window_button_values(settings["config"]))


if __name__ == "__main__":
    main()
