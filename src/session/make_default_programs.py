"""Writes the session's choice of which program opens which kind of file.

    python make_default_programs.py SHELL.json OUTPUT-FOLDER

Each [[default_programs]] table in the config names MIME types and the
programs for them, in order of preference:

    [[default_programs]]
    kinds = ["image/png", "image/jpeg"]
    programs = ["org.kde.gwenview.desktop", "org.gnome.Loupe.desktop"]

Written as OUTPUT-FOLDER/mimeapps.list, which start-desktop adds to
XDG_CONFIG_DIRS. Without it, the first registered program in alphabetical
order wins, so a web browser often opens pictures. The user's own
~/.config/mimeapps.list still comes first; uninstalled programs are skipped.
"""

import json
import sys
from pathlib import Path


def read_groups(shell_settings_file):
    """The config's [[default_programs]] tables, checked."""
    config = json.loads(Path(shell_settings_file).read_text())["config"]
    groups = config.get("default_programs", [])
    for number, group in enumerate(groups, start=1):
        where = f"[[default_programs]] number {number}"
        for key in ("kinds", "programs"):
            entries = group.get(key)
            if not isinstance(entries, list) or not entries:
                raise SystemExit(f"{where}: '{key}' must be a list with at least one entry.")
        for program in group["programs"]:
            if not str(program).endswith(".desktop"):
                raise SystemExit(
                    f"{where}: '{program}' is not the name of a desktop entry "
                    "(it should end in .desktop, like org.kde.gwenview.desktop)."
                )
    return groups


def write_mimeapps_list(groups, output_folder):
    lines = ["[Default Applications]"]
    for group in groups:
        programs = ";".join(group["programs"]) + ";"
        for kind in group["kinds"]:
            lines.append(f"{kind}={programs}")
    Path(output_folder).mkdir(parents=True, exist_ok=True)
    (Path(output_folder) / "mimeapps.list").write_text("\n".join(lines) + "\n")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: make_default_programs.py SHELL.json OUTPUT-FOLDER")
    groups = read_groups(sys.argv[1])
    if groups:
        write_mimeapps_list(groups, sys.argv[2])


if __name__ == "__main__":
    main()
