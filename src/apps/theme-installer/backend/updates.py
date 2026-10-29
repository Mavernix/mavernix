"""A look's updates: what changed in its themes since they were imported.

A look's updates.toml lists its updates by revision (its comment says how).
Two numbers in an imported theme's [theme] table say how far it has come:

    revision     the update the import was made from; written by the import.
                 A theme from before revisions were kept has none (0).
    updated_to   the update whose settings and corrections were last applied
                 to it at login (refresh). Never lower than `revision`.

What an update's `settings` and `corrections` give, any theme can take at
login without a new import (refresh, run by src/session/refresh_themes.py),
once: a value the user changes afterwards stays. What its `import_brings`
lists only a new import gives; the Theme Installer offers that import, once
per such update (offers_import).

The user's answers are kept in ~/.config/mavernix/theme-installer.json:
`later`, the newest revision with something to import that the user put off,
for each look, and `last_installer`, the installer last imported from.

Both the session and the Theme Installer use this module, so the rules live
in one place.
"""

import json
import os
import re
import sys
import tomllib
from pathlib import Path

THEME_FILE = "theme.toml"
UPDATES_FILE = "updates.toml"

# A table's header line, such as `[dock.folder.grid]`, perhaps with a comment
# after it; not `[[...]]`, which starts an entry of a list of tables.
TABLE_HEADER = re.compile(r"^\s*\[([^\[\]]+)\]\s*(#.*)?$")
# The start of a key's line, such as `icon_size =`.
KEY_LINE = re.compile(r"^\s*([A-Za-z0-9_-]+)\s*=")


class UpdatesProblem(ValueError):
    """Something in updates.toml or a theme file this module cannot use."""


# --- updates.toml --------------------------------------------------------------

def read_updates(look_folder):
    """The look's updates, oldest first; [] for a look without updates.toml."""
    updates_file = Path(look_folder) / UPDATES_FILE
    if not updates_file.is_file():
        return []
    with open(updates_file, "rb") as file:
        updates = tomllib.load(file).get("update", [])
    for update in updates:
        if not isinstance(update.get("revision"), int):
            raise UpdatesProblem(f"{updates_file}: every [[update]] needs a whole-number revision")
    return sorted(updates, key=lambda update: update["revision"])


def newest_revision(updates):
    return max((update["revision"] for update in updates), default=0)


def newest_import_revision(updates):
    """The newest update that brings something only a new import can give."""
    return max((update["revision"] for update in updates if update.get("import_brings")), default=0)


# --- Editing a theme file --------------------------------------------------------

def value_text(value):
    """A value as TOML, on one line. updates.toml holds only these kinds.
    (src/session/toml_text.py writes whole files for the session.)"""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return repr(value)
    if isinstance(value, str):
        # JSON's escapes are all valid in a TOML basic string.
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, list):
        return "[" + ", ".join(value_text(item) for item in value) + "]"
    if isinstance(value, dict):
        return "{ " + ", ".join(f"{key} = {value_text(item)}" for key, item in value.items()) + " }"
    raise UpdatesProblem(f"cannot write a {type(value).__name__} into a theme")


class ThemeFile:
    """A theme.toml edited line by line, so its comments and order stay.

    Only tables written under a header of their own can be changed; a table
    written inline or with dotted keys is refused rather than duplicated.
    save() writes nothing that does not read back as TOML.
    """

    def __init__(self, path):
        self.path = Path(path)
        self.lines = self.path.read_text().splitlines(keepends=True)
        if self.lines and not self.lines[-1].endswith("\n"):
            self.lines[-1] += "\n"
        self.data = tomllib.loads("".join(self.lines))

    def table(self, dotted_name):
        """The table's values as read, or None if the theme lacks it."""
        table = self.data
        for part in dotted_name.split("."):
            table = table.get(part)
            if not isinstance(table, dict):
                return None
        return table

    def header_index(self, dotted_name):
        for index, line in enumerate(self.lines):
            match = TABLE_HEADER.match(line)
            if match and match.group(1).strip() == dotted_name:
                return index
        return None

    def section_end(self, header):
        """The index of the first line after the table's own lines."""
        for index in range(header + 1, len(self.lines)):
            if self.lines[index].lstrip().startswith("["):
                return index
        return len(self.lines)

    def set(self, dotted_name, key, value):
        """Gives the key this value, replacing its line or adding one."""
        new_line = f"{key} = {value_text(value)}\n"
        header = self.header_index(dotted_name)
        if header is None:
            if self.table(dotted_name) is not None:
                raise UpdatesProblem(f"{self.path}: [{dotted_name}] is not written under a header of "
                                     "its own, so it is left as it is")
            self.lines += ["\n", f"[{dotted_name}]\n", new_line]
        else:
            end = self.section_end(header)
            for index in range(header + 1, end):
                match = KEY_LINE.match(self.lines[index])
                if match and match.group(1) == key:
                    self.lines[index] = new_line
                    break
            else:
                self.lines.insert(header + 1, new_line)
        self.data = tomllib.loads("".join(self.lines))

    def save(self):
        text = "".join(self.lines)
        tomllib.loads(text)
        # So that a session starting meanwhile never reads half a file.
        fresh = self.path.with_name(f".{self.path.name}.new")
        fresh.write_text(text)
        fresh.replace(self.path)


def theme_revision(theme_dir):
    """The update the theme was imported from: 0 before revisions were kept."""
    with open(Path(theme_dir) / THEME_FILE, "rb") as file:
        return tomllib.load(file).get("theme", {}).get("revision", 0)


def stamp_revision(theme_dir, revision):
    """Writes [theme] revision into a freshly made theme."""
    theme = ThemeFile(Path(theme_dir) / THEME_FILE)
    if theme.header_index("theme") is None:
        raise UpdatesProblem(f"{theme.path} has no [theme] table to write its revision into")
    theme.set("theme", "revision", revision)
    theme.save()


# --- Refreshing imported themes at login -----------------------------------------

def refresh_theme(theme_file, updates):
    """Applies the updates the theme has not had yet; whether it changed.

    Settings are added where the theme lacks them, corrections made where the
    theme still holds the value they replace. An update with a `themes` list
    gives them only to the themes it names (the folder's name). Afterwards
    [theme] updated_to records the newest update, so none is applied twice.
    """
    name = Path(theme_file).parent.name
    theme = ThemeFile(theme_file)
    marks = theme.table("theme") or {}
    applied_to = max(marks.get("revision", 0), marks.get("updated_to", 0))
    pending = [update for update in updates if update["revision"] > applied_to]
    if not pending:
        return False
    for update in pending:
        if name not in update.get("themes", [name]):
            continue
        for correction in update.get("corrections", []):
            table = theme.table(correction["table"])
            if table is not None and table.get(correction["key"]) == correction["was"]:
                theme.set(correction["table"], correction["key"], correction["now"])
        for table_name, values in update.get("settings", {}).items():
            table = theme.table(table_name) or {}
            for key, value in values.items():
                if key not in table:
                    theme.set(table_name, key, value)
    theme.set("theme", "updated_to", newest_revision(updates))
    theme.save()
    return True


def refresh(looks_dir, themes_dir):
    """Brings the user's imported themes up to date with each look's updates;
    the names of the themes changed."""
    changed = []
    for look_file in sorted(Path(looks_dir).glob("*/look.toml")):
        try:
            with open(look_file, "rb") as file:
                themes = tomllib.load(file).get("themes", [])
            updates = read_updates(look_file.parent)
        except (OSError, ValueError) as error:
            print(f"refresh_themes: leaving out {look_file.parent.name}: {error}", file=sys.stderr)
            continue
        for name in themes:
            theme_file = Path(themes_dir) / name / THEME_FILE
            # Only themes the user imported.
            if not updates or not theme_file.is_file():
                continue
            try:
                if refresh_theme(theme_file, updates):
                    changed.append(name)
            except (OSError, KeyError, ValueError) as error:
                print(f"refresh_themes: leaving {name} as it is: {error}", file=sys.stderr)
    return changed


# --- Offering a new import ----------------------------------------------------------

def import_brings(look, themes_dir):
    """What a new import would bring an imported look, one sentence each: []
    when its themes are up to date or not imported at all."""
    installed = [Path(themes_dir) / name for name in look["themes"]
                 if (Path(themes_dir) / name / THEME_FILE).is_file()]
    if not installed:
        return []
    oldest = min(theme_revision(theme_dir) for theme_dir in installed)
    sentences = []
    for update in read_updates(look["folder"]):
        if update["revision"] > oldest:
            sentences += update.get("import_brings", [])
    return sentences


def offers_import(look_id, look, themes_dir):
    """Whether the Theme Installer should offer a new import of the look: it
    would bring something, and the user has not put off the newest such
    update."""
    if not import_brings(look, themes_dir):
        return False
    return not is_put_off(look_id, newest_import_revision(read_updates(look["folder"])))


def state_path():
    config_home = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config")
    return config_home / "mavernix" / "theme-installer.json"


def read_state():
    try:
        return json.loads(state_path().read_text())
    except (OSError, ValueError):
        return {}


def write_state(state):
    path = state_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    fresh = path.with_name(f".{path.name}.new")
    fresh.write_text(json.dumps(state, indent=2) + "\n")
    fresh.replace(path)


def put_off(revisions):
    """Remembers that the user said "Later": {look id: newest revision with
    something to import}. All looks at once, in one write."""
    state = read_state()
    state.setdefault("later", {}).update(revisions)
    write_state(state)


def is_put_off(look_id, revision):
    return read_state().get("later", {}).get(look_id, 0) >= revision


def remember_installer(source):
    state = read_state()
    state["last_installer"] = str(source)
    write_state(state)


def last_installer():
    """The installer last imported from, if it is still there; else ""."""
    path = read_state().get("last_installer", "")
    return path if path and Path(path).exists() else ""
