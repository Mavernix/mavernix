"""What the App Store offers: the apps a GitHub organisation publishes.

An app is a repository of the organisation that carries the app topic (both
come from the desktop config's [app_store]). Everything the store shows comes
from the repository's latest release:

    mavernix-app.toml     the listing: name, description, icon, screenshots,
                          and where the program is inside the bundle
    the files it names    the icon and screenshots
    NAME-x86_64.tar.gz    the bundle the store installs (for this machine's
                          processor, as `uname -m` names it)

Extensions (docs/extensions.md) are offered from one repository of the
organisation. Its latest release brings:

    extensions.toml       the list: a table for each extension (name, summary,
                          version, icon, ...), see read_extension_index()
    the files it names    the icons and screenshots
    NAME.tar.gz           each extension's folder, in a folder called NAME/

How to publish an app is in docs/app-store.md.
"""

import platform
import re
import sys
import tomllib
from pathlib import Path

import web

LISTING_FILE = "mavernix-app.toml"

# What a listing must say, and what it may.
REQUIRED_KEYS = ("title", "summary", "icon", "command", "desktop_entry")
OPTIONAL_TEXT_KEYS = ("description", "category", "developer", "license")


EXTENSION_INDEX_FILE = "extensions.toml"
EXTENSION_REQUIRED_KEYS = ("title", "summary", "version", "icon")
# An extension's name and version end up in file names, so they can't hold a
# slash or start with a dot.
SAFE_NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*")


def bundle_name(app_name):
    return f"{app_name}-{platform.machine()}.tar.gz"


def extension_bundle_name(extension_name):
    # Extensions are QML and small programs, the same on every processor.
    return f"{extension_name}.tar.gz"


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


def read_extension_index(text, source):
    """{name: its entry} from the list in `text` (an extensions.toml), checked.
    An entry that isn't usable is left out, with the reason on standard error."""
    try:
        index = tomllib.loads(text)
    except tomllib.TOMLDecodeError as error:
        raise web.StoreProblem(f"{source} isn't valid TOML: {error}.") from error

    entries = {}
    for name, entry in index.items():
        try:
            if not SAFE_NAME.fullmatch(name):
                raise web.StoreProblem("its name has to be letters, digits, - and _")
            if not isinstance(entry, dict):
                raise web.StoreProblem("it has to be a table")
            for key in EXTENSION_REQUIRED_KEYS:
                if not isinstance(entry.get(key), str) or not entry[key].strip():
                    raise web.StoreProblem(f"it needs a `{key}`")
            if not SAFE_NAME.fullmatch(entry["version"]):
                raise web.StoreProblem("its `version` has to be letters, digits, . - and _")
            for key in OPTIONAL_TEXT_KEYS:
                if not isinstance(entry.get(key, ""), str):
                    raise web.StoreProblem(f"`{key}` has to be text")
            screenshots = entry.get("screenshots", [])
            if not isinstance(screenshots, list) or not all(isinstance(file, str) for file in screenshots):
                raise web.StoreProblem("`screenshots` has to be a list of file names")
        except web.StoreProblem as problem:
            print(f"app-store: leaving out the extension {name}: {problem}", file=sys.stderr)
            continue
        entries[name] = entry
    return entries


def find_extensions(organisation, repository_name):
    """Every extension the organisation's extensions repository offers, sorted
    by title, in the shape of find_apps()' apps (with "kind" set to
    "extension", and no command or desktop entry). There are none if the
    repository has no release yet. An extension the release can't back up is left out, with
    the reason on standard error."""
    repository = f"{organisation}/{repository_name}"
    try:
        release = web.get_json(f"repos/{repository}/releases/latest")
    except web.StoreProblem as problem:
        if problem.status != 404:
            raise
        # A 404 also answers for a repository that isn't there (a misspelt
        # name, or a private one), which the store shouldn't pass over quietly.
        listed = web.get_json(f"orgs/{organisation}/repos?per_page=100")
        if repository_name not in {listed_repository["name"] for listed_repository in listed}:
            raise web.StoreProblem(f"{organisation} has no repository called {repository_name}.") from problem
        return []
    tag = release.get("tag_name")
    if not tag:
        raise web.StoreProblem(f"The latest release of {repository_name} has no tag.")
    assets = {asset["name"]: asset for asset in release.get("assets", [])}

    # The list and the pictures are kept per release: a new release fetches them anew.
    files = web.cache_folder() / "extensions" / web.safe_file_name(tag)
    index_file = fetch_release_file(assets, tag, files, EXTENSION_INDEX_FILE)
    index = read_extension_index(index_file.read_text(), f"{repository_name}'s {EXTENSION_INDEX_FILE}")

    extensions = []
    for name, entry in index.items():
        try:
            bundle = assets.get(extension_bundle_name(name))
            if bundle is None:
                raise web.StoreProblem(f"its release {tag} has no {extension_bundle_name(name)}")
            extensions.append({
                "kind": "extension",
                "name": name,
                "title": entry["title"],
                "summary": entry["summary"],
                "description": entry.get("description", "").strip(),
                "category": entry.get("category", ""),
                "developer": entry.get("developer", organisation),
                "license": entry.get("license", ""),
                "icon": str(fetch_release_file(assets, tag, files, entry["icon"])),
                "screenshots": [str(fetch_release_file(assets, tag, files, file_name))
                                for file_name in entry.get("screenshots", [])],
                "command": "",
                "desktop_entry": "",
                "version": entry["version"],
                "repository": f"https://github.com/{repository}",
                **release_details(release, bundle),
            })
        except (web.StoreProblem, KeyError, TypeError, ValueError, OSError) as problem:
            print(f"app-store: leaving out the extension {name}: {problem}", file=sys.stderr)
    return sorted(extensions, key=lambda extension: extension["title"].lower())


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


def fetch_release_file(assets, tag, folder, file_name):
    """The file `file_name` of a release (`assets` by name, `tag` for the words
    of a problem), kept in `folder`; fetched if it isn't there yet."""
    if file_name not in assets:
        raise web.StoreProblem(f"release {tag} has no {file_name}")
    kept = folder / file_name
    if not kept.is_file():
        web.download(assets[file_name]["browser_download_url"], kept)
    return kept


def release_details(release, bundle):
    """What a listing says of its release and of `bundle`, the asset that gets installed."""
    return {
        "released": release.get("published_at", "")[:10],
        "whats_new": (release.get("body") or "").strip(),
        "bundle_url": bundle["browser_download_url"],
        "bundle_size": bundle["size"],
        # GitHub gives each file's checksum as "sha256:…"; without one, the
        # bundle can't be checked, and installing it is refused (bundles.py).
        "bundle_sha256": (bundle.get("digest") or "").removeprefix("sha256:"),
    }


def describe_app(repository):
    """What the store shows of one repository: its listing, and what its latest release brings."""
    name = repository["name"]
    try:
        release = web.get_json(f"repos/{repository['full_name']}/releases/latest")
    except web.StoreProblem as problem:
        raise web.StoreProblem(f"it has no release yet ({problem})") from problem
    assets = {asset["name"]: asset for asset in release.get("assets", [])}
    tag = release["tag_name"]
    version = tag.removeprefix("v")

    # The listing and its pictures are kept per version: a new release fetches them anew.
    files = web.cache_folder() / "apps" / name / web.safe_file_name(version)

    def release_file(file_name):
        return fetch_release_file(assets, tag, files, file_name)

    listing = read_listing(release_file(LISTING_FILE).read_text(), f"{name}'s {LISTING_FILE}")
    bundle = assets.get(bundle_name(name))
    if bundle is None:
        raise web.StoreProblem(f"its release {tag} has no {bundle_name(name)}")

    return {
        "kind": "app",
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
        "repository": repository["html_url"],
        **release_details(release, bundle),
    }
