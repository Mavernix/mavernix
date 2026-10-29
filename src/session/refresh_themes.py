#!/usr/bin/env python3
"""Gives the themes the Theme Installer made what later versions changed.

    refresh_themes.py

Run at every login. The rules (which update a theme has had, what it may
take without a new import) are the Theme Installer's, in its updates.py,
so that the installer and the session never disagree about them.
"""

import os
import sys
from pathlib import Path

from paths import LIBRARY_DIR

INSTALLER_DIR = LIBRARY_DIR / "theme-installer"
LOOKS_DIR = INSTALLER_DIR / "looks"


def main():
    themes = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share") / "mavernix" / "themes"
    if not LOOKS_DIR.is_dir() or not themes.is_dir():
        return
    sys.path.insert(0, str(INSTALLER_DIR))
    import updates

    for name in updates.refresh(LOOKS_DIR, themes):
        print(f"refresh_themes: brought {name} up to date with this version", file=sys.stderr)


if __name__ == "__main__":
    main()
