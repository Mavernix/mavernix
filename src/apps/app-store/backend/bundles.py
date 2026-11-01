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

An extension is just its folder, put in ~/.config/mavernix/extensions/NAME/,
where the desktop looks for the user's own. The store leaves a file in it,
.app-store.json (the same record as an app's installed.json), and its icon;
a folder without that file is the user's and is never replaced or removed.
The desktop reads its extensions when it starts, so a new one shows up after
the next login. It is switched on in the settings window, as any other.
"""

import json
import os
import shlex
import shutil
import sys
import tarfile
import tomllib
from pathlib import Path

import web

STORE_KEY = "X-Mavernix-App-Store"


def data_folder():
    return Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local" / "share")


def apps_folder():
    return data_folder() / "mavernix" / "apps"


def extensions_folder():
    config = os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config"
    return Path(config) / "mavernix" / "extensions"


EXTENSION_RECORD = ".app-store.json"


def _read_records(folder, record_file):
    """{folder name: the record in its record_file} for the folders that have one.
    A record that can't be read is left out, with the reason on standard error."""
    found = {}
    if folder.is_dir():
        for record_path in sorted(folder.glob(f"*/{record_file}")):
            # Not .NAME-new or .NAME-old, which an install leaves behind if it is stopped halfway.
            if record_path.parent.name.startswith("."):
                continue
            try:
                record = json.loads(record_path.read_text())
                record["listing"]["name"]
            except (OSError, ValueError, KeyError, TypeError) as problem:
                print(f"app-store: ignoring {record_path}: {problem!r}", file=sys.stderr)
                continue
            found[record_path.parent.name] = record
    return found


def installed_apps():
    """{name: what installed.json holds} for every installed app."""
    found = _read_records(apps_folder(), "installed.json")
    for record in found.values():
        # Apps installed before the store had extensions have no kind in their listing.
        record["listing"].setdefault("kind", "app")
    return found


def installed_extensions():
    """{name: what .app-store.json holds} for every extension the store installed."""
    return _read_records(extensions_folder(), EXTENSION_RECORD)


def installed_command(name, record=None):
    """The installed program of the app `name`, as a full path; `record` is
    its installed.json, if already read."""
    record = record or installed_apps()[name]
    return str(apps_folder() / name / "bundle" / record["listing"]["command"])


def icon_file_name(app):
    """The name its folder gives the icon of `app` (a listing) once it is installed."""
    suffix = Path(app["icon"]).suffix
    return ".app-store-icon" + suffix if app["kind"] == "extension" else "icon" + suffix


def installed_icon(app):
    """Where the icon of `app` (a listing) is kept once it is installed."""
    folder = extensions_folder() if app["kind"] == "extension" else apps_folder()
    return folder / app["name"] / icon_file_name(app)


def install(app, on_progress):
    """Installs `app` (one of catalog.find_apps()), or updates it to that version.

    on_progress(step, fraction) is called with step "downloading" (fraction
    from 0 to 1) and then "installing".
    """
    name = app["name"]
    if not app["bundle_sha256"]:
        raise web.StoreProblem(f"GitHub gave no checksum for {app['title']}'s download, so it can't be checked.")

    if app["kind"] == "extension":
        _refuse_users_own_extension(app)

    download = web.cache_folder() / "downloads" / f"{app['kind']}-{name}-{web.safe_file_name(app['version'])}.tar.gz"
    try:
        web.download(
            app["bundle_url"],
            download,
            sha256=app["bundle_sha256"],
            on_progress=lambda done, total: on_progress("downloading", done / (total or app["bundle_size"] or 1)),
        )
        on_progress("installing", 1.0)
        if app["kind"] == "extension":
            _install_extension(app, download)
        else:
            _install_app(app, download)
    finally:
        download.unlink(missing_ok=True)


def _install_app(app, download):
    name = app["name"]
    # Everything is put together in a new folder first; the old version stays until the swap.
    new_folder = apps_folder() / f".{name}-new"
    shutil.rmtree(new_folder, ignore_errors=True)
    new_folder.mkdir(parents=True)
    try:
        _unpack(download, new_folder, name, renamed_to="bundle")
        bundle = new_folder / "bundle"
        for key in ("command", "desktop_entry"):
            if not (bundle / app[key]).is_file():
                raise web.StoreProblem(f"{app['title']}'s download has no {app[key]}, which its listing promises.")
        shutil.copyfile(app["icon"], new_folder / icon_file_name(app))
        record = {"version": app["version"], "listing": app}
        (new_folder / "installed.json").write_text(json.dumps(record, indent=2))
    except BaseException:
        shutil.rmtree(new_folder, ignore_errors=True)
        raise

    _swap_in(new_folder, apps_folder() / name)
    _write_desktop_entry(app)


def _refuse_users_own_extension(app):
    """Before anything is downloaded: a folder the store didn't make is the user's."""
    folder = extensions_folder() / app["name"]
    if folder.exists() and not (folder / EXTENSION_RECORD).is_file():
        raise web.StoreProblem(f"You have an extension called {app['name']} of your own; the store won't replace it.")


def _install_extension(app, download):
    name = app["name"]
    folder = extensions_folder() / name

    # The extension's files are unpacked in a folder of a folder, so the
    # desktop doesn't take the half-done one for an extension.
    staging = extensions_folder() / f".{name}-new"
    shutil.rmtree(staging, ignore_errors=True)
    staging.mkdir(parents=True)
    try:
        _unpack(download, staging, name)
        new_folder = staging / name
        _check_manifest(app, new_folder / "extension.toml")
        shutil.copyfile(app["icon"], new_folder / icon_file_name(app))
        (new_folder / EXTENSION_RECORD).write_text(json.dumps({"version": app["version"], "listing": app}, indent=2))
        _swap_in(new_folder, folder)
    finally:
        shutil.rmtree(staging, ignore_errors=True)


def _check_manifest(app, manifest_file):
    """The desktop leaves out an extension whose extension.toml it can't read,
    with only a line in its log; better not to install such a one."""
    if not manifest_file.is_file():
        raise web.StoreProblem(f"{app['title']}'s download has no extension.toml.")
    try:
        tomllib.loads(manifest_file.read_text())
    except ValueError as error:
        raise web.StoreProblem(f"{app['title']}'s extension.toml isn't valid: {error}.") from error


def remove(name):
    """Removes the app or extension `name`, and the app's desktop entry."""
    app = installed_apps().get(name)
    if app is not None:
        _remove_app(name, app)
    elif name in installed_extensions():
        shutil.rmtree(extensions_folder() / name)
    else:
        # An extension folder without the store's record is the user's own,
        # and isn't found as installed, so it is never removed.
        raise web.StoreProblem(f"{name} isn't installed.")


def _remove_app(name, record):
    entry = _entry_path(record["listing"])
    if entry.is_file() and f"{STORE_KEY}={name}" in entry.read_text().splitlines():
        entry.unlink()
    shutil.rmtree(apps_folder() / name)


def _unpack(archive, folder, name, renamed_to=None):
    """Unpacks the bundle, whose files all sit in a folder called NAME, into
    `folder`, as folder/NAME or, if given, folder/renamed_to."""
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
    if renamed_to is not None:
        (folder / name).rename(folder / renamed_to)


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

