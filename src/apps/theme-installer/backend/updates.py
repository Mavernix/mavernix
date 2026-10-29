"""What a new import of a look would bring, and what the user said about it.

A look's updates.toml lists its updates by revision (its comment says how).
The themes an import makes record the newest revision ([theme] revision); an
older theme, or one from before revisions were kept, misses the updates
after its own. Those whose `import_brings` says something are worth offering
a new import for; their settings alone reach the theme at login without one
(src/session/refresh_themes.py).

The user's answers are kept in ~/.config/mavernix/theme-installer.json:
`later`, the newest revision of each look the user put off (it is offered
again when a newer one comes), and `last_installer`, the installer last
imported from, so that importing again can start with it.
"""

import json
import os
import tomllib
from pathlib import Path

THEME_FILE = "theme.toml"


def read_updates(look):
    """The look's updates, oldest first; [] for a look without updates.toml."""
    updates_file = look["folder"] / "updates.toml"
    if not updates_file.is_file():
        return []
    with open(updates_file, "rb") as file:
        return sorted(tomllib.load(file).get("update", []), key=lambda update: update["revision"])


def newest_revision(look):
    return max((update["revision"] for update in read_updates(look)), default=0)


def theme_revision(theme_dir):
    """The revision a theme was made from: 0 before revisions were kept."""
    with open(theme_dir / THEME_FILE, "rb") as file:
        return tomllib.load(file).get("theme", {}).get("revision", 0)


def stamp_revision(theme_dir, revision):
    """Writes [theme] revision into a freshly made theme."""
    theme_file = theme_dir / THEME_FILE
    lines = theme_file.read_text().splitlines(keepends=True)
    lines = [line for line in lines if not line.startswith("revision = ")]
    header = lines.index("[theme]\n")
    lines.insert(header + 1, f"revision = {revision}\n")
    theme_file.write_text("".join(lines))


def import_brings(look, themes_dir):
    """What a new import would bring an imported look, one sentence each: []
    when its themes are up to date or not imported at all."""
    installed = [themes_dir / name for name in look["themes"] if (themes_dir / name / THEME_FILE).is_file()]
    if not installed:
        return []
    oldest = min(theme_revision(theme_dir) for theme_dir in installed)
    sentences = []
    for update in read_updates(look):
        if update["revision"] > oldest:
            sentences += update.get("import_brings", [])
    return sentences


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


def put_off(look_id, revision):
    """Remembers that the user said "Later" to the look's updates up to REVISION."""
    state = read_state()
    state.setdefault("later", {})[look_id] = revision
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
