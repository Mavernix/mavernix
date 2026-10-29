"""Installing, updating and removing apps, for the user alone (no password needed).

Each installed app gets a folder in ~/.local/share/mavernix/apps/:

    NAME/bundle/          the release's bundle, unpacked as it came
    NAME/installed.json   which version it is, and its listing (catalog.py)
    NAME/icon.EXT         the listing's icon

and a desktop entry in ~/.local/share/applications/, copied from the bundle
with Exec and Icon pointing into that folder. The entry puts the app in the
app grid, and lets the dock recognise its windows. The store marks the entry
as its own (X-Mavernix-App-Store), so removing an app never deletes an entry
the user made.

An update unpacks the new version beside the old one and swaps the two at the
end, so a failed update leaves the old version as it was.
"""

import json
import os
import shlex
import shutil
import tarfile
from pathlib import Path

import web

STORE_KEY = "X-Mavernix-App-Store"


def data_folder():
    return Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share")


def apps_folder():
    return data_folder() / "mavernix" / "apps"


def installed_apps():
    """{name: what installed.json holds} for every installed app."""
    found = {}
    if apps_folder().is_dir():
        for record in sorted(apps_folder().glob("*/installed.json")):
            # Not .NAME-new or .NAME-old, which an install leaves behind if it is stopped halfway.
            if record.parent.name.startswith("."):
                continue
            found[record.parent.name] = json.loads(record.read_text())
    return found


def installed_command(name, record=None):
    """The installed program of the app `name`, as a full path; `record` is
    its installed.json, if already read."""
    record = record or installed_apps()[name]
    return str(apps_folder() / name / "bundle" / record["listing"]["command"])


def installed_icon(app):
    """Where the icon of `app` (a listing) is kept once it is installed."""
    return apps_folder() / app["name"] / ("icon" + Path(app["icon"]).suffix)


def install(app, on_progress):
    """Installs `app` (one of catalog.find_apps()), or updates it to that version.

    on_progress(step, fraction) is called with step "downloading" (fraction
    from 0 to 1) and then "installing".
    """
    name = app["name"]
    if not app["bundle_sha256"]:
        raise web.StoreProblem(f"GitHub gave no checksum for {app['title']}'s download, so it can't be checked.")

    download = web.cache_folder() / "downloads" / f"{name}-{app['version']}.tar.gz"
    web.download(
        app["bundle_url"],
        download,
        sha256=app["bundle_sha256"],
        on_progress=lambda done, total: on_progress("downloading", done / (total or app["bundle_size"] or 1)),
    )
    on_progress("installing", 1.0)

    # Everything is put together in a new folder first; the old version stays until the swap.
    new_folder = apps_folder() / f".{name}-new"
    shutil.rmtree(new_folder, ignore_errors=True)
    new_folder.mkdir(parents=True)
    try:
        _unpack(download, new_folder, name)
        bundle = new_folder / "bundle"
        for key in ("command", "desktop_entry"):
            if not (bundle / app[key]).is_file():
                raise web.StoreProblem(f"{app['title']}'s download has no {app[key]}, which its listing promises.")
        shutil.copyfile(app["icon"], new_folder / installed_icon(app).name)
        record = {"version": app["version"], "listing": app}
        (new_folder / "installed.json").write_text(json.dumps(record, indent=2))
    except BaseException:
        shutil.rmtree(new_folder, ignore_errors=True)
        raise

    _swap_in(new_folder, apps_folder() / name)
    download.unlink()
    _write_desktop_entry(app)


def remove(name):
    """Removes the app `name` and its desktop entry."""
    folder = apps_folder() / name
    record = installed_apps().get(name)
    if record is None:
        raise web.StoreProblem(f"{name} isn't installed.")
    entry = _entry_path(record["listing"])
    if entry.is_file() and f"{STORE_KEY}={name}" in entry.read_text().splitlines():
        entry.unlink()
    shutil.rmtree(folder)


def _unpack(archive, folder, name):
    """Unpacks the bundle, whose files all sit in a folder called NAME, as folder/bundle."""
    with tarfile.open(archive) as bundle:
        top_folders = {Path(member.name).parts[0] for member in bundle.getmembers()}
        if top_folders != {name}:
            raise web.StoreProblem(f"The download should hold one folder, {name}, but holds {sorted(top_folders)}.")
        # The "data" filter refuses anything that would land outside the folder,
        # device files, and set-user-ID programs.
        try:
            bundle.extractall(folder, filter="data")
        except tarfile.FilterError as error:
            raise web.StoreProblem(f"The download holds a file the App Store won't unpack: {error}.") from error
    (folder / name).rename(folder / "bundle")


def _swap_in(new_folder, folder):
    old_folder = folder.with_name(f".{folder.name}-old")
    shutil.rmtree(old_folder, ignore_errors=True)
    if folder.exists():
        folder.rename(old_folder)
    new_folder.rename(folder)
    shutil.rmtree(old_folder, ignore_errors=True)


def _entry_path(app):
    # Named like the bundle's own entry: the dock matches windows to entries by that name.
    return data_folder() / "applications" / Path(app["desktop_entry"]).name


def _write_desktop_entry(app):
    """Copies the bundle's desktop entry, pointing Exec and Icon at the installed app."""
    folder = apps_folder() / app["name"]
    command = str(folder / "bundle" / app["command"])
    icon = str(installed_icon(app))
    entry = _entry_path(app)
    # An entry by that name that the store didn't write is the user's own: it
    # stays, as remove() leaves it. Its Exec still works, as the installed
    # program keeps its path from version to version.
    if entry.is_file() and f"{STORE_KEY}={app['name']}" not in entry.read_text().splitlines():
        return

    lines = []
    in_main_group = False
    for line in (folder / "bundle" / app["desktop_entry"]).read_text().splitlines():
        if line.startswith("["):
            in_main_group = line.strip() == "[Desktop Entry]"
        elif in_main_group and line.startswith("Exec="):
            # Keep the arguments (%U and the like); only the program changes.
            arguments = shlex.split(line.removeprefix("Exec="))[1:]
            line = "Exec=" + " ".join(_exec_quote(argument) for argument in [command, *arguments])
        elif in_main_group and line.startswith("Icon="):
            line = f"Icon={icon}"
        elif in_main_group and line.startswith("TryExec="):
            continue
        lines.append(line)
        if line.strip() == "[Desktop Entry]":
            lines.append(f"{STORE_KEY}={app['name']}")

    entry.parent.mkdir(parents=True, exist_ok=True)
    entry.write_text("\n".join(lines) + "\n")


def _exec_quote(argument):
    """`argument` as the Desktop Entry spec quotes it in Exec: in double quotes
    (not shell's single ones) when it needs quoting, with \\ escaped once
    more for the file's own string escapes."""
    if argument and not any(character in argument for character in ' \t\n"\'\\><~|&;$*?#()`'):
        return argument
    quoted = '"' + "".join("\\" + c if c in '"`$\\' else c for c in argument) + '"'
    return quoted.replace("\\", "\\\\")

