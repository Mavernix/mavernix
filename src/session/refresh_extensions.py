"""Writes the extensions the desktop finds now, so a running session follows
an extension installed or removed without logging out.

    refresh_extensions.py SHELL_SETTINGS [--enable NAME]

SHELL_SETTINGS is the session's shell.json (make_shell_config.py). The list
goes to the file it names under `extensions_file`, in the form of its own
`extensions`; the shell and the settings window read that file and follow
it. With --enable, the running shell is then asked to switch NAME on. The
shell keeps the user's choices, so it writes them, not this script.

configure-desktop runs this at login; the App Store, through
mavernix-extensions-changed, after it installs or removes an extension.

The shell keeps a QML file's code for as long as it runs, by its path, so an
extension's new files would never run under its old paths. Each extension's
files are therefore also reached through a link named after them
(`load_directory`, in `extension-versions` beside the list): an update or any
other change to the files gives it a new name, and the shell loads that.
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from make_shell_config import found_extensions
from paths import SHELL_DIR

# Long enough for a busy shell; the extension is in place whatever happens.
SHELL_TIMEOUT_SECONDS = 10


def write_atomically(path, data):
    """So that a reader never finds half a file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", dir=path.parent, prefix=f".{path.name}-", delete=False) as file:
        json.dump(data, file, indent=1)
        file.write("\n")
    os.replace(file.name, path)


def fingerprint(directory):
    """Changes whenever a file in `directory` is added, removed, changed or
    replaced (the App Store swaps in a new folder, whose files are new ones)."""
    seen = hashlib.sha256()
    for folder, subfolders, files in os.walk(directory):
        subfolders.sort()
        for name in sorted(files):
            path = Path(folder, name)
            try:
                status = path.lstat()
            except OSError:
                continue
            about = (path.relative_to(directory), status.st_ino, status.st_size, status.st_mtime_ns)
            seen.update("\0".join(map(str, about)).encode() + b"\n")
    return seen.hexdigest()[:16]


def link_versions(found, links_folder):
    """Gives each extension in `found` its `load_directory`, a link in
    `links_folder` named after its files.

    Old links stay until the session ends (the folder is in its runtime
    folder): a shell still running an old version may load more of its files,
    and another run of this script at the same time may have just listed one."""
    links_folder.mkdir(parents=True, exist_ok=True)
    for name, extension in found.items():
        link = links_folder / f"{name}-{fingerprint(extension['directory'])}"
        try:
            link.symlink_to(extension["directory"])
        except FileExistsError:
            pass
        extension["load_directory"] = str(link)


def switch_on(name):
    """Asks the running shell to switch the extension `name` on (Extensions.qml);
    what went wrong, or ""."""
    command = ["qs", "-p", str(SHELL_DIR), "ipc", "call", "extensions", "switchOn", name]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=SHELL_TIMEOUT_SECONDS)
    except (OSError, subprocess.TimeoutExpired) as error:
        return str(error)
    if result.returncode != 0:
        # qs gives some reasons, such as finding no shell, on its output.
        return result.stderr.strip() or result.stdout.strip() or "it gave no reason"
    return ""


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("settings", type=Path, help="the session's shell.json")
    parser.add_argument("--enable", metavar="NAME", help="also have the shell switch the extension NAME on")
    arguments = parser.parse_args()

    settings = json.loads(arguments.settings.read_text())
    if "extensions_file" not in settings:
        sys.exit("refresh_extensions: this session started before the desktop could follow new extensions; "
                 "they show at the next login")
    found = found_extensions(settings["config"], Path(settings["theme_directory"]).name)
    extensions_file = Path(settings["extensions_file"])
    link_versions(found, extensions_file.with_name("extension-versions"))
    write_atomically(extensions_file, {"extensions": found})

    if arguments.enable is None:
        return
    if arguments.enable not in found:
        sys.exit(f"refresh_extensions: there is no extension called {arguments.enable} "
                 "(or its files can't be read; the reason is above)")
    problem = switch_on(arguments.enable)
    if problem:
        sys.exit(f"refresh_extensions: the shell couldn't switch {arguments.enable} on: {problem}")


if __name__ == "__main__":
    main()
