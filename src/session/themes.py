"""Finds a theme by name, and puts together a theme that builds on another.

A theme may say `based_on = "NAME"` (docs/themes/README.md): it is that theme
with its own keys laid over, as with desktop configs. The programs that read
a theme (the compositor's window frames, the shell, the toolkit themes) all
expect one folder with one theme.toml, so such a theme is put together into
one: the joined theme.toml, and links to the files of every theme in the
chain, the upper theme's file where both have one. A picture named by the
base theme is then found beside the upper theme's file, by the same path.

Tables are joined key by key; a value written inline (a fill, a font) is
taken whole from the upper theme, or `{ image = … }` over `{ color = … }`
would keep both. Which is which, TOML does not keep once read: the theme
format's list of tables (theme-keys.json, made from docs/themes/) says.
"""

import json
import os
import shutil
import sys
import tomllib
from pathlib import Path

from paths import DATA_DIR, THEMES_DIR
from toml_text import toml_text

THEME_FILE = "theme.toml"
FORMAT_KEYS_FILE = DATA_DIR / "theme-keys.json"

# Icon themes tried, in order, when the one a theme names is not installed.
# hicolor is the base every system has; it holds only programs' own icons.
FALLBACK_ICON_THEMES = ["Adwaita", "breeze", "Papirus", "hicolor"]


def fail(message):
    sys.exit(f"themes: {message}")


def user_themes_dir():
    """The user's own themes, which come before the installed ones."""
    data_home = os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share"
    return Path(data_home) / "mavernix" / "themes"


def theme_folders():
    """Where themes are looked for: the user's own first, then the installed ones."""
    return [user_themes_dir(), THEMES_DIR]


def joined_themes_dir():
    """Where themes built on another are put together; made anew at every start."""
    cache_home = os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache"
    return Path(cache_home) / "mavernix" / "joined-themes"


def theme_directory(name):
    """The folder of the theme called `name` as installed, or None if there is none."""
    for parent in theme_folders():
        candidate = parent / name
        if (candidate / THEME_FILE).is_file():
            return candidate
    return None


def read_theme(folder):
    try:
        with open(folder / THEME_FILE, "rb") as file:
            return tomllib.load(file)
    except tomllib.TOMLDecodeError as error:
        fail(f"{folder / THEME_FILE}: {error}")


def theme_chain(name):
    """The folders of the theme and of every theme below it, the theme's own first."""
    chain = []
    while name is not None:
        folder = theme_directory(name)
        if folder is None:
            searched = " or ".join(str(parent) for parent in theme_folders())
            below = f' (below "{chain[-1].name}")' if chain else ""
            fail(f'theme "{name}"{below} not found in {searched}')
        if folder in chain:
            fail(f"{folder / THEME_FILE} is based on itself, through based_on")
        chain.append(folder)
        name = read_theme(folder).get("based_on")
    return chain


def theme_available(name):
    """Whether the theme called `name`, and every theme it is based on, is there."""
    seen = []
    while name is not None:
        folder = theme_directory(name)
        if folder is None or folder in seen:
            return False
        seen.append(folder)
        name = read_theme(folder).get("based_on")
    return True


def format_tables():
    try:
        format_keys = json.loads(FORMAT_KEYS_FILE.read_text())
    except FileNotFoundError:
        fail(f"{FORMAT_KEYS_FILE} is missing; it is made when the desktop is built")
    return set(format_keys["tables"])


def joined(base, upper, tables, path=""):
    """`upper` laid over `base`: the tables in `tables` key by key, anything else whole."""
    result = dict(base)
    for key, value in upper.items():
        dotted = f"{path}.{key}" if path else key
        if dotted in tables and isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = joined(result[key], value, tables, dotted)
        else:
            result[key] = value
    return result


def icon_theme_folders(theme_dir):
    """Where icon themes are looked for, as the icon theme specification
    lists them, and the theme's own share/icons/."""
    data_home = os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share"
    data_dirs = (os.environ.get("XDG_DATA_DIRS") or "/usr/local/share:/usr/share").split(":")
    folders = [Path.home() / ".icons", Path(data_home) / "icons"]
    folders += [Path(folder) / "icons" for folder in data_dirs if folder]
    folders.append(Path(theme_dir) / "share" / "icons")
    return folders


def usable_icon_theme(theme, theme_dir):
    """`theme` with its [icons] theme replaced by an installed one if the one
    it names is missing, so that programs do not show empty squares."""
    wanted = theme.get("icons", {}).get("theme")
    if wanted is None:
        return theme
    folders = icon_theme_folders(theme_dir)

    def installed(name):
        return any((folder / name / "index.theme").is_file() for folder in folders)

    if installed(wanted):
        return theme
    usable = next((name for name in FALLBACK_ICON_THEMES if installed(name)), "hicolor")
    print(f"themes: the icon theme \"{wanted}\" is not installed; using \"{usable}\" instead",
          file=sys.stderr)
    return dict(theme, icons=dict(theme["icons"], theme=usable))


def link_files(layers, target):
    """Links into `target` what the folders in `layers` hold, the first
    layer's file where several have one. A folder that only one layer has is
    linked whole: an icon theme holds thousands of files."""
    target.mkdir(parents=True, exist_ok=True)
    names = {}
    for layer in reversed(layers):
        for entry in layer.iterdir():
            names.setdefault(entry.name, []).insert(0, entry)
    for name, entries in names.items():
        if name == THEME_FILE:
            continue
        folders = [entry for entry in entries if entry.is_dir()]
        if len(folders) > 1 and len(folders) == len(entries):
            link_files(folders, target / name)
        else:
            (target / name).symlink_to(entries[0].resolve())


def find_theme(name):
    """The folder to read the theme called `name` from; ends the program if
    the theme, or one it is based on, is missing."""
    chain = theme_chain(name)
    if len(chain) == 1:
        return chain[0]

    tables = format_tables()
    theme = {}
    for folder in reversed(chain):
        theme = joined(theme, read_theme(folder), tables)
    theme.pop("based_on", None)

    folder = joined_themes_dir() / name
    # Made beside the old one and then put in its place, so a program that
    # reads the theme meanwhile finds the old one whole.
    fresh = folder.with_name(name + ".new")
    shutil.rmtree(fresh, ignore_errors=True)
    link_files(chain, fresh)
    (fresh / THEME_FILE).write_text(toml_text(theme))
    shutil.rmtree(folder, ignore_errors=True)
    fresh.rename(folder)
    return folder
