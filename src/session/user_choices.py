"""The user's own changes to a desktop config, and laying them over it.

The shell writes what the user changes to $XDG_CONFIG_HOME/mavernix/NAME.json,
holding only what differs, shaped like the config:

    { "dock": { "groups": [["foot", "featherpad"]] } }

A config with `based_on = "sea-lion"` is laid over sea-lion.toml and shares its
choices file, so the user's settings survive switching between the two.
"""

import json
import os
import sys
import tomllib
from pathlib import Path

from paths import CONFIG_DIR


def settings_home():
    """The folder that holds what the user has chosen about the desktop."""
    config_home = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return Path(config_home) / "mavernix"


def default_desktop_path():
    """One line naming the desktop that starts by default."""
    return settings_home() / "desktop"


def read_config_file(config_path):
    """One desktop config file, as it is written (without its base)."""
    try:
        with open(config_path, "rb") as file:
            return tomllib.load(file)
    except OSError as error:
        sys.exit(f"user_choices: cannot read the desktop config {config_path}: {error.strerror}")
    except tomllib.TOMLDecodeError as error:
        sys.exit(f"user_choices: {config_path} is not valid TOML: {error}")


def user_config_dir():
    """Desktops the Theme Installer added, for this user only."""
    data_home = os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share"
    return Path(data_home) / "mavernix" / "config"


def find_desktop_config(name, beside=None):
    """The config file of the desktop called `name`, or None: next to
    `beside` if given, then the user's own, then the installed ones."""
    folders = [user_config_dir(), CONFIG_DIR]
    if beside is not None:
        folders.insert(0, Path(beside).parent)
    for folder in folders:
        candidate = folder / f"{name}.toml"
        if candidate.is_file():
            return candidate
    return None


def desktop_names():
    """Every desktop there is a config for."""
    return sorted({path.stem for folder in (user_config_dir(), CONFIG_DIR) for path in folder.glob("*.toml")})


def base_of(config_path, config):
    """The config file a config is based on, or None."""
    name = config.get("based_on")
    if name is None:
        return None
    base = find_desktop_config(name, beside=config_path)
    if base is None:
        sys.exit(f"user_choices: {config_path} is based on \"{name}\", but there is no desktop of that name")
    return base


def root_config_path(config_path):
    """The config at the bottom of a chain of based_on, the one with no base."""
    path = Path(config_path)
    seen = {path.resolve()}
    while True:
        base = base_of(path, read_config_file(path))
        if base is None:
            return path
        if base.resolve() in seen:
            sys.exit(f"user_choices: {config_path} is based on itself, through based_on")
        seen.add(base.resolve())
        path = base


def load_desktop_config(config_path):
    """A desktop config with what it is based on under it (see the top)."""
    # Exits on a looping chain, which would otherwise recurse forever.
    root_config_path(config_path)
    config = read_config_file(config_path)
    base = base_of(config_path, config)
    own_keys = {key: value for key, value in config.items() if key != "based_on"}
    if base is None:
        return own_keys
    return laid_over(load_desktop_config(base), own_keys)


def choices_path(config_path):
    """Where the user's changes to the desktop config at `config_path` are kept.

    A desktop based on another keeps them in that other desktop's file.
    """
    return settings_home() / (root_config_path(config_path).stem + ".json")


def laid_over(defaults, changes):
    """Returns `defaults` with `changes` laid over it.

    A table in both is joined key by key; anything else in `changes`, a list
    included, takes the place of what `defaults` had.
    """
    result = dict(defaults)
    for key, changed in changes.items():
        if isinstance(changed, dict) and isinstance(result.get(key), dict):
            result[key] = laid_over(result[key], changed)
        else:
            result[key] = changed
    return result


def with_user_choices(config, config_path):
    """Returns the desktop config with the user's changes, if any, laid over it.

    A damaged file is reported and skipped rather than stopping the desktop.
    """
    path = choices_path(config_path)
    if not path.is_file():
        return config
    try:
        with open(path) as file:
            changes = json.load(file)
        if not isinstance(changes, dict):
            raise ValueError("it does not hold a table")
    except (OSError, ValueError) as error:
        print(
            f"user_choices: leaving out {path}: {error}. "
            "The desktop starts with its defaults; delete or repair the file.",
            file=sys.stderr,
        )
        return config
    return laid_over(config, changes)
