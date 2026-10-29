#!/usr/bin/env python3
"""Gives the themes the Theme Installer made the settings later versions added.

    refresh_themes.py

A look's updates.toml lists, for each revision, theme settings that need
nothing new from the installer. Run at every login, this adds the ones an
imported theme lacks, so that it works with this version of the desktop
without a new import. A value already in the theme is never changed. What
only a new import can bring, the Theme Installer offers instead.

A theme made after an update records its revision ([theme] revision) and
already has that update's settings, so it is left alone.
"""

import os
import sys
import tomllib
from pathlib import Path

from paths import LIBRARY_DIR
from toml_text import key_text, value_text

LOOKS_DIR = LIBRARY_DIR / "theme-installer" / "looks"
THEME_FILE = "theme.toml"


def read_toml(path):
    with open(path, "rb") as file:
        return tomllib.load(file)


def settings_since(updates, revision):
    """{table name: {key: value}} from every update newer than REVISION."""
    settings = {}
    for update in updates:
        if update.get("revision", 0) <= revision:
            continue
        for table, values in update.get("settings", {}).items():
            settings.setdefault(table, {}).update(values)
    return settings


def table_in(theme, dotted_name):
    """The table a dotted name such as "dock.folder.grid" names, or None."""
    table = theme
    for part in dotted_name.split("."):
        table = table.get(part)
        if not isinstance(table, dict):
            return None
    return table


def add_settings(theme_file, settings):
    """Writes the settings the theme lacks into it; whether it changed.

    The keys go right below their table's header, so the rest of the file,
    comments included, stays as it was. A table the theme does not have at
    all is added at the end.
    """
    text = theme_file.read_text()
    theme = tomllib.loads(text)
    lines = text.splitlines(keepends=True)
    if lines and not lines[-1].endswith("\n"):
        lines[-1] += "\n"
    changed = False
    for table_name, values in settings.items():
        table = table_in(theme, table_name) or {}
        new_lines = [f"{key_text(key)} = {value_text(value)}\n"
                     for key, value in values.items() if key not in table]
        if not new_lines:
            continue
        header = f"[{table_name}]"
        position = next((index for index, line in enumerate(lines) if line.strip() == header), None)
        if position is None:
            lines += ["\n", header + "\n"] + new_lines
        else:
            lines[position + 1:position + 1] = new_lines
        changed = True
    if changed:
        # So that a session starting meanwhile never reads half a file.
        fresh = theme_file.with_name(f".{theme_file.name}.new")
        fresh.write_text("".join(lines))
        fresh.replace(theme_file)
    return changed


def refresh(looks_dir, themes_dir):
    """Adds each look's newer settings to the user's imported themes; the
    names of the themes changed."""
    changed = []
    for look_file in sorted(looks_dir.glob("*/look.toml")):
        updates_file = look_file.parent / "updates.toml"
        if not updates_file.is_file():
            continue
        try:
            themes = read_toml(look_file).get("themes", [])
            updates = read_toml(updates_file).get("update", [])
        except (OSError, ValueError) as error:
            print(f"refresh_themes: leaving out {look_file.parent.name}: {error}", file=sys.stderr)
            continue
        for name in themes:
            theme_file = themes_dir / name / THEME_FILE
            # Only themes the user imported.
            if not theme_file.is_file():
                continue
            try:
                revision = read_toml(theme_file).get("theme", {}).get("revision", 0)
                settings = settings_since(updates, revision)
                if settings and add_settings(theme_file, settings):
                    changed.append(name)
            except (OSError, ValueError) as error:
                print(f"refresh_themes: leaving {name} as it is: {error}", file=sys.stderr)
    return changed


def main():
    themes = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share") / "mavernix" / "themes"
    if LOOKS_DIR.is_dir() and themes.is_dir():
        for name in refresh(LOOKS_DIR, themes):
            print(f"refresh_themes: gave {name} this version's new settings", file=sys.stderr)


if __name__ == "__main__":
    main()
