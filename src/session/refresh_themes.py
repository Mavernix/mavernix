#!/usr/bin/env python3
"""Gives the themes the Theme Installer made what later versions changed.

    refresh_themes.py

A look's updates.toml lists, for each revision, what an imported theme can
take without a new import: settings to add, and corrections of values an
older import wrote. Run at every login, this applies the ones a theme lacks,
so that it works with this version of the desktop without importing again.
What only a new import can bring, the Theme Installer offers instead.

A value already in the theme is never overwritten, except by a correction,
and then only while it still holds what the import wrote: a value changed by
hand stays. A theme made after an update records its revision ([theme]
revision) and is left alone for that update.
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


def updates_since(updates, revision):
    """From every update newer than REVISION: the settings to add, as
    {table name: {key: value}}, and the corrections, as a list."""
    settings, corrections = {}, []
    for update in updates:
        if update.get("revision", 0) <= revision:
            continue
        for table, values in update.get("settings", {}).items():
            settings.setdefault(table, {}).update(values)
        corrections += update.get("corrections", [])
    return settings, corrections


def table_in(theme, dotted_name):
    """The table a dotted name such as "dock.folder.grid" names, or None."""
    table = theme
    for part in dotted_name.split("."):
        table = table.get(part)
        if not isinstance(table, dict):
            return None
    return table


def header_line(lines, table_name):
    """The index of a table's header line, or None."""
    header = f"[{table_name}]"
    return next((index for index, line in enumerate(lines) if line.strip() == header), None)


def add_settings(lines, theme, settings):
    """Adds the settings the theme lacks, right below their table's header
    (a table the theme lacks goes at the end); whether anything was added."""
    changed = False
    for table_name, values in settings.items():
        table = table_in(theme, table_name) or {}
        new_lines = [f"{key_text(key)} = {value_text(value)}\n"
                     for key, value in values.items() if key not in table]
        if not new_lines:
            continue
        position = header_line(lines, table_name)
        if position is None:
            lines += ["\n", f"[{table_name}]\n"] + new_lines
        else:
            lines[position + 1:position + 1] = new_lines
        changed = True
    return changed


def correct_values(lines, theme, corrections):
    """Replaces each correction's `was` with `now` where the theme still
    holds `was`; whether anything was replaced."""
    changed = False
    for correction in corrections:
        table_name, key = correction["table"], correction["key"]
        table = table_in(theme, table_name)
        if table is None or table.get(key) != correction["was"]:
            continue
        position = header_line(lines, table_name)
        if position is None:
            continue
        # The key's line is in the table's part of the file, before the next header.
        for index in range(position + 1, len(lines)):
            line = lines[index]
            if line.lstrip().startswith("["):
                break
            name, separator, _ = line.partition("=")
            if separator and name.strip() == key_text(key):
                lines[index] = f"{key_text(key)} = {value_text(correction['now'])}\n"
                changed = True
                break
    return changed


def update_theme(theme_file, settings, corrections):
    """Applies the settings and corrections to one theme file, keeping the
    rest of it, comments included, as it was; whether it changed."""
    text = theme_file.read_text()
    theme = tomllib.loads(text)
    lines = text.splitlines(keepends=True)
    if lines and not lines[-1].endswith("\n"):
        lines[-1] += "\n"
    corrected = correct_values(lines, theme, corrections)
    added = add_settings(lines, theme, settings)
    if not (corrected or added):
        return False
    # So that a session starting meanwhile never reads half a file.
    fresh = theme_file.with_name(f".{theme_file.name}.new")
    fresh.write_text("".join(lines))
    fresh.replace(theme_file)
    return True


def refresh(looks_dir, themes_dir):
    """Brings the user's imported themes up to date with each look's
    updates; the names of the themes changed."""
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
                settings, corrections = updates_since(updates, revision)
                if (settings or corrections) and update_theme(theme_file, settings, corrections):
                    changed.append(name)
            except (OSError, ValueError, KeyError) as error:
                print(f"refresh_themes: leaving {name} as it is: {error}", file=sys.stderr)
    return changed


def main():
    themes = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share") / "mavernix" / "themes"
    if LOOKS_DIR.is_dir() and themes.is_dir():
        for name in refresh(LOOKS_DIR, themes):
            print(f"refresh_themes: brought {name} up to date with this version", file=sys.stderr)


if __name__ == "__main__":
    main()
