"""Writes the one file the shell reads its settings from.

    python make_shell_config.py data/config/plain.toml OUTPUT.json

The shell cannot read TOML and should not know where themes are kept, so
this resolves both and writes one JSON file:

    {
      "config": { ... the desktop config ... },
      "default_config": { ... the same without the user's changes, for Settings' resets ... },
      "shortcut_defaults": { "shortcuts": { ... }, "commands": { ... } },
      "system_keyboard": { "layout": "us", "variant": "intl", ... },
      "theme": { ... the theme's theme.toml, with the icon theme the user picked ... },
      "theme_directory": "/full/path/of/the/theme",
      "theme_icon_theme": "SeaLion",
      "shell_icon_theme": "SeaLion",
      "user_choices_file": "/full/path/of/the/user's/changes.json",
      "desktop": "sea-lion",
      "desktops": ["plain", "sea-lion", "sea-lion-dark"],
      "desktops_sharing_choices": ["sea-lion", "sea-lion-dark"],
      "default_desktop_file": "/full/path/of/the/user's/choice/of/desktop",
      "user": { "name": "me", "full_name": "My Name", "picture": "/home/me/.face" },
      "extensions": { "NAME": { "directory": "...", "manifest": { ... }, "looks": { ... }, "settings": { ... },
                              "decimal_settings": [ ... ] } },
      "extensions_file": "/full/path/of/extensions.json"
    }

The theme's [icons] theme is replaced by the one the user picked for
programs (the config's [icons] programs); `shell_icon_theme` is the one for
the shell itself, the Dock and menu bar ([icons] dock_and_menu_bar), and
`theme_icon_theme` the theme's own, for Settings to offer. Both are empty if
the theme names none and the user picked none.
`config` includes the user's changes (user_choices.py); `shortcut_defaults`
is without them, for "Restore Default". `extensions` lists every extension
found, enabled or not (docs/extensions.md); each one's `decimal_settings` names the
settings whose default has a decimal point, since a JSON reader takes 1.0 for 1.
`extensions_file`, beside the output, holds the same list as found later
(refresh_extensions.py), so a running session follows an extension installed
or removed; `extensions` is the list as the session started.
Nothing else is interpreted here.
"""

import argparse
import json
import os
import pwd
import sys
import tomllib
from pathlib import Path

from make_wayfire_config import load_toml, system_keyboard
from paths import EXTENSIONS_DIR
from themes import find_theme, usable_icon_theme
from user_choices import (choices_path, default_desktop_path, desktop_names, desktops_sharing_choices, load_desktop_config,
                          with_user_choices)


def user_details():
    """The user's login name, full name and picture, for the lock screen.

    The picture is ~/.face or the AccountsService icon, as login screens use.
    """
    account = pwd.getpwuid(os.getuid())
    full_name = account.pw_gecos.split(",")[0]
    picture = ""
    for candidate in (Path(account.pw_dir) / ".face",
                      Path(account.pw_dir) / ".face.icon",
                      Path("/var/lib/AccountsService/icons") / account.pw_name):
        if candidate.is_file():
            picture = str(candidate)
            break
    return {"name": account.pw_name, "full_name": full_name, "picture": picture}


def extension_folders():
    """Where extensions are looked for: the user's own first, then bundled ones."""
    own = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") / "mavernix" / "extensions"
    return [own, EXTENSIONS_DIR]


def found_extensions(config, theme_name):
    """Every extension, by folder name.

    Looks come from looks/THEME.toml or looks/default.toml; settings are the
    manifest's [settings] under the config's [extensions.NAME]. A broken
    extension is reported and left out rather than stopping the desktop.
    """
    found = {}
    for folder in extension_folders():
        if not folder.is_dir():
            continue
        for directory in sorted(folder.iterdir()):
            manifest_file = directory / "extension.toml"
            # The user's own wins over a bundled one of the same name. A
            # folder starting with a dot is an install or update left half-done.
            if directory.name.startswith(".") or not manifest_file.is_file() or directory.name in found:
                continue
            # Not load_toml, which exits on an unreadable file.
            try:
                with open(manifest_file, "rb") as file:
                    manifest = tomllib.load(file)
                looks = {}
                for looks_file in (directory / "looks" / f"{theme_name}.toml", directory / "looks" / "default.toml"):
                    if looks_file.is_file():
                        with open(looks_file, "rb") as file:
                            looks = tomllib.load(file)
                        break
            except (OSError, ValueError) as error:
                print(f"make_shell_config: leaving out the extension in {directory}: {error}", file=sys.stderr)
                continue
            settings = dict(manifest.get("settings", {}))
            decimal_settings = [key for key, default in settings.items() if isinstance(default, float)]
            wishes = config.get("extensions", {}).get(directory.name, {})
            if isinstance(wishes, dict):
                settings.update(wishes)
            found[directory.name] = {
                "directory": str(directory),
                "manifest": manifest,
                "looks": looks,
                "settings": settings,
                "decimal_settings": decimal_settings,
            }
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("config", type=Path, help="desktop config, e.g. data/config/plain.toml")
    parser.add_argument("output", type=Path, help="JSON file to write")
    arguments = parser.parse_args()

    default_config = load_desktop_config(arguments.config)
    config = with_user_choices(default_config, arguments.config)
    if "theme" not in config:
        sys.exit(f"make_shell_config: {arguments.config} does not name a theme")
    theme_dir = find_theme(config["theme"])
    theme = load_toml(theme_dir / "theme.toml")
    # The icon themes picked in Settings, App Style, each in the theme's place.
    picked_icons = config.get("icons", {})
    shell_theme = usable_icon_theme(theme, theme_dir, picked_icons.get("dock_and_menu_bar"))

    settings = {
        "config": config,
        "default_config": default_config,
        "shortcut_defaults": {
            "shortcuts": default_config.get("shortcuts", {}),
            "commands": default_config.get("commands", {}),
        },
        "system_keyboard": system_keyboard(),
        "theme": usable_icon_theme(theme, theme_dir, picked_icons.get("programs")),
        "theme_directory": str(theme_dir),
        "theme_icon_theme": theme.get("icons", {}).get("theme", ""),
        "shell_icon_theme": shell_theme.get("icons", {}).get("theme", ""),
        "user_choices_file": str(choices_path(arguments.config)),
        "desktop": arguments.config.stem,
        "desktops": desktop_names(),
        # Those with the same user_choices_file: what [theme_schedule] can switch between.
        "desktops_sharing_choices": desktops_sharing_choices(arguments.config),
        "default_desktop_file": str(default_desktop_path()),
        "user": user_details(),
        "extensions": found_extensions(config, theme_dir.name),
        "extensions_file": str(arguments.output.resolve().with_name("extensions.json")),
    }
    # The shell writes that file but does not make folders.
    choices_path(arguments.config).parent.mkdir(parents=True, exist_ok=True)

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    with open(arguments.output, "w") as output:
        json.dump(settings, output, indent=1)
    print(f"wrote {arguments.output} (theme: {theme_dir.name})")


if __name__ == "__main__":
    main()
