"""The App Store's back end, run by the window (store.cpp) once for each thing it does:

    app_store.py catalog ORGANISATION TOPIC [EXTENSIONS_REPOSITORY]
    app_store.py install ORGANISATION TOPIC NAME [EXTENSIONS_REPOSITORY]
    app_store.py remove NAME

It reports on standard output, one JSON object per line:

    {"event": "catalog", "apps": [...],       every app and extension offered ("kind" says which),
                                              see catalog.py; each also has
     "problem": "..."}                        "installed_version" and "installed_command"
                                              ("" when it isn't installed). When GitHub can't
                                              be asked, `problem` says why, and the apps are
                                              only the installed ones; otherwise it is "".
    {"event": "progress", "step": "downloading" or "installing", "fraction": 0.42}
    {"event": "done"}                         install or remove worked
    {"event": "failed", "message": "..."}     what went wrong, for the user to read

Anything else it has to say goes to standard error, which the window logs.
"""

import json
import sys
import time

import bundles
import catalog
import web

# The window redraws its progress bar at most this often.
PROGRESS_INTERVAL_SECONDS = 0.1


def report(event, **details):
    print(json.dumps({"event": event, **details}), flush=True)


def find_everything(organisation, topic, extensions_repository=""):
    """(the apps, then the extensions, that the organisation offers; why the
    extensions couldn't be read, or ""). An app and an extension of one name
    can't both be installed; the app is the one kept. Without a repository
    for extensions, there are none."""
    offered = catalog.find_apps(organisation, topic)
    if not extensions_repository:
        return offered, ""
    try:
        extensions = catalog.find_extensions(organisation, extensions_repository)
    # The apps still show when the extensions can't be read.
    except web.StoreProblem as problem:
        return offered, f"The extensions couldn't be read: {problem}"
    taken = {app["name"] for app in offered}
    return offered + [extension for extension in extensions if extension["name"] not in taken], ""


def apps_with_installed_versions(apps):
    """`apps` (from the catalog), with what is installed added to them."""
    installed = {"app": bundles.installed_apps(), "extension": bundles.installed_extensions()}
    # An installed app or extension that is no longer offered still shows, as
    # its listing was when it was installed, so it can be opened and removed.
    # Names are shared by both kinds: an extension whose name an offered app
    # has taken stays hidden (and installed) rather than show up twice.
    offered = {app["name"] for app in apps}
    for kind, records in installed.items():
        apps += [{**record["listing"], "icon": str(bundles.installed_icon(record["listing"]))}
                 for name, record in records.items() if name not in offered]
        offered |= set(records)
    apps.sort(key=lambda app: app["title"].lower())
    for app in apps:
        record = installed[app["kind"]].get(app["name"])
        app["installed_version"] = record["version"] if record else ""
        app["installed_command"] = bundles.installed_command(app["name"], record) if record and app["kind"] == "app" else ""
    return apps


def install(organisation, topic, name, extensions_repository=""):
    offered, problem = find_everything(organisation, topic, extensions_repository)
    apps = {app["name"]: app for app in offered}
    if name not in apps:
        # Better the reason the extensions weren't read than "not offered".
        raise web.StoreProblem(problem or f"{name} isn't offered in the App Store (anymore).")

    last_report = 0.0

    def on_progress(step, fraction):
        nonlocal last_report
        now = time.monotonic()
        if step != "downloading" or now - last_report >= PROGRESS_INTERVAL_SECONDS:
            report("progress", step=step, fraction=round(min(fraction, 1.0), 3))
            last_report = now

    bundles.install(apps[name], on_progress)


def main(arguments):
    command, *rest = arguments or [""]
    if command == "catalog" and len(rest) in (2, 3):
        # Without GitHub (offline, or past its limit) the installed apps still
        # show, so they can be opened and removed.
        try:
            offered, problem = find_everything(*rest)
        except web.StoreProblem as error:
            offered, problem = [], str(error)
        report("catalog", apps=apps_with_installed_versions(offered), problem=problem)
    elif command == "install" and len(rest) in (3, 4):
        install(*rest)
        report("done")
    elif command == "remove" and len(rest) == 1:
        bundles.remove(*rest)
        report("done")
    else:
        print(__doc__, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except web.StoreProblem as problem:
        report("failed", message=str(problem))
        sys.exit(1)
