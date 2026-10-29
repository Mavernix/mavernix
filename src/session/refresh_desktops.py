#!/usr/bin/env python3
"""Brings the desktops the Theme Installer added up to date with this version.

    refresh_desktops.py

Importing a look (Sea Lion, say) copies its desktop configs into the user's
folder, where an update of the desktop never reaches them: renamed or new
wording would stay old until the look is imported again. Run at every
login, this copies a newer config over the user's. The themes themselves,
made from the installer the user dropped, are not touched.

An import records what it copied (.NAME.toml.installed, as import_theme.py
writes it). A copy that no longer matches that record was changed by hand,
or comes from an import before records were kept, and is kept beside the
new one as NAME.toml.before-update.
"""

import hashlib
import os
import sys
import tomllib
from pathlib import Path

from paths import LIBRARY_DIR

LOOKS_DIR = LIBRARY_DIR / "theme-installer" / "looks"


def fingerprint(content):
    return hashlib.sha256(content).hexdigest()


def refresh(looks_dir, configs_dir):
    """Copies each look's newer desktop configs over the user's copies; the
    names of those replaced."""
    replaced = []
    for look_file in sorted(looks_dir.glob("*/look.toml")):
        try:
            with open(look_file, "rb") as file:
                desktops = tomllib.load(file).get("desktops", [])
        except (OSError, ValueError) as error:
            print(f"refresh_desktops: leaving out {look_file.parent.name}: {error}", file=sys.stderr)
            continue
        for desktop in desktops:
            bundled = look_file.parent / f"{desktop}.toml"
            copy = configs_dir / f"{desktop}.toml"
            # Only looks the user imported.
            if not copy.is_file() or not bundled.is_file():
                continue
            new = bundled.read_bytes()
            old = copy.read_bytes()
            if new == old:
                continue
            record = configs_dir / f".{desktop}.toml.installed"
            if not record.is_file() or record.read_text().strip() != fingerprint(old):
                (configs_dir / f"{desktop}.toml.before-update").write_bytes(old)
            # So that a session starting meanwhile never reads half a file.
            fresh = configs_dir / f".{desktop}.toml.new"
            fresh.write_bytes(new)
            fresh.replace(copy)
            record.write_text(fingerprint(new) + "\n")
            replaced.append(desktop)
    return replaced


def main():
    configs = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share") / "mavernix" / "config"
    if LOOKS_DIR.is_dir() and configs.is_dir():
        for desktop in refresh(LOOKS_DIR, configs):
            print(f"refresh_desktops: brought {desktop} up to date", file=sys.stderr)


if __name__ == "__main__":
    main()
