"""The App Store's back end, run by the window (store.cpp) once for each thing it does:

    app_store.py catalog ORGANISATION TOPIC
    app_store.py install ORGANISATION TOPIC NAME
    app_store.py remove NAME

It reports on standard output, one JSON object per line:

    {"event": "catalog", "apps": [...],       every app offered, see catalog.py; each also has
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


def apps_with_installed_versions(apps):
    """`apps` (from the catalog), with what is installed added to them."""
    installed = bundles.installed_apps()
    # An installed app that is no longer offered still shows, as its listing
    # was when it was installed, so it can be opened and removed.
    offered = {app["name"] for app in apps}
    apps += [{**record["listing"], "icon": str(bundles.installed_icon(record["listing"]))}
             for name, record in installed.items() if name not in offered]
    apps.sort(key=lambda app: app["title"].lower())
    for app in apps:
        record = installed.get(app["name"])
        app["installed_version"] = record["version"] if record else ""
        app["installed_command"] = bundles.installed_command(app["name"], record) if record else ""
    return apps


def install(organisation, topic, name):
    apps = {app["name"]: app for app in catalog.find_apps(organisation, topic)}
    if name not in apps:
        raise web.StoreProblem(f"{name} isn't offered in the App Store (anymore).")

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
    if command == "catalog" and len(rest) == 2:
        # Without GitHub (offline, or past its limit) the installed apps still
        # show, so they can be opened and removed.
        try:
            offered, problem = catalog.find_apps(*rest), ""
        except web.StoreProblem as error:
            offered, problem = [], str(error)
        report("catalog", apps=apps_with_installed_versions(offered), problem=problem)
    elif command == "install" and len(rest) == 3:
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
