"""What the App Store offers: the apps a GitHub organisation publishes.

An app is a repository of the organisation that carries the app topic (both
come from the desktop config's [app_store]). Everything the store shows comes
from the repository's latest release:

    mavernix-app.toml     the listing: name, description, icon, screenshots,
                          and where the program is inside the bundle
    the files it names    the icon and screenshots
    NAME-x86_64.tar.gz    the bundle the store installs (for this machine's
                          processor, as `uname -m` names it)

How to publish an app is in docs/app-store.md.
"""

import platform
import sys
import tomllib
from pathlib import Path

import web

LISTING_FILE = "mavernix-app.toml"

# What a listing must say, and what it may.
REQUIRED_KEYS = ("title", "summary", "icon", "command", "desktop_entry")
OPTIONAL_TEXT_KEYS = ("description", "category", "developer", "license")


def bundle_name(app_name):
    return f"{app_name}-{platform.machine()}.tar.gz"


def read_listing(text, source):
    """The listing in `text` (a mavernix-app.toml), checked; `source` names it in problems."""
    try:
        listing = tomllib.loads(text)
    except tomllib.TOMLDecodeError as error:
        raise web.StoreProblem(f"{source} isn't valid TOML: {error}.") from error

    for key in REQUIRED_KEYS:
        if not isinstance(listing.get(key), str) or not listing[key].strip():
            raise web.StoreProblem(f"{source} needs a `{key}`.")
    for key in OPTIONAL_TEXT_KEYS:
        if not isinstance(listing.get(key, ""), str):
            raise web.StoreProblem(f"{source}: `{key}` has to be text.")
    screenshots = listing.get("screenshots", [])
    if not isinstance(screenshots, list) or not all(isinstance(name, str) for name in screenshots):
        raise web.StoreProblem(f"{source}: `screenshots` has to be a list of file names.")
    # The program and its entry are opened inside the installed bundle, so
    # they may not point out of it.
    for key in ("command", "desktop_entry"):
        if Path(listing[key]).is_absolute() or ".." in Path(listing[key]).parts:
            raise web.StoreProblem(f"{source}: `{key}` has to be a path inside the bundle.")
    return listing


def find_apps(organisation, topic):
    """Every app the organisation offers, sorted by title. Repositories whose
    release can't be used are left out, with the reason on standard error."""
    repositories = web.get_json(f"orgs/{organisation}/repos?per_page=100")
    apps = []
    for repository in repositories:
        if topic not in repository.get("topics", []) or repository.get("archived"):
            continue
        try:
            apps.append(describe_app(repository))
        # One release that isn't what the store expects (a key GitHub left
        # out, a listing that isn't UTF-8) leaves out that app, not the store.
        except (web.StoreProblem, KeyError, TypeError, ValueError, OSError) as problem:
            print(f"app-store: leaving out {repository['name']}: {problem}", file=sys.stderr)
    return sorted(apps, key=lambda app: app["title"].lower())


def describe_app(repository):
    """What the store shows of one repository: its listing, and what its latest release brings."""
    name = repository["name"]
    try:
        release = web.get_json(f"repos/{repository['full_name']}/releases/latest")
    except web.StoreProblem as problem:
        raise web.StoreProblem(f"it has no release yet ({problem})") from problem
    assets = {asset["name"]: asset for asset in release.get("assets", [])}
    version = release["tag_name"].removeprefix("v")

    # The listing and its pictures are kept per version: a new release fetches them anew.
    files = web.cache_folder() / "apps" / name / version

    def release_file(file_name):
        if file_name not in assets:
            raise web.StoreProblem(f"its release {release['tag_name']} has no {file_name}")
        kept = files / file_name
        if not kept.is_file():
            web.download(assets[file_name]["browser_download_url"], kept)
        return kept

    listing = read_listing(release_file(LISTING_FILE).read_text(), f"{name}'s {LISTING_FILE}")
    bundle = assets.get(bundle_name(name))
    if bundle is None:
        raise web.StoreProblem(f"its release {release['tag_name']} has no {bundle_name(name)}")

    return {
        "name": name,
        "title": listing["title"],
        "summary": listing["summary"],
        "description": listing.get("description", "").strip(),
        "category": listing.get("category", ""),
        "developer": listing.get("developer", repository["owner"]["login"]),
        "license": listing.get("license", ""),
        "icon": str(release_file(listing["icon"])),
        "screenshots": [str(release_file(file_name)) for file_name in listing.get("screenshots", [])],
        "command": listing["command"],
        "desktop_entry": listing["desktop_entry"],
        "version": version,
        "released": release.get("published_at", "")[:10],
        "whats_new": (release.get("body") or "").strip(),
        "repository": repository["html_url"],
        "bundle_url": bundle["browser_download_url"],
        "bundle_size": bundle["size"],
        # GitHub gives each file's checksum as "sha256:…"; without one, the
        # bundle can't be checked, and installing it is refused (bundles.py).
        "bundle_sha256": (bundle.get("digest") or "").removeprefix("sha256:"),
    }
