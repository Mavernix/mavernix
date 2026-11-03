"""The App Store's back end (src/apps/app-store/backend): which apps it finds,
how it installs, updates and removes them, and what it refuses.

A local web server stands in for GitHub: it answers the API's questions from
files in a temporary folder and serves a small made-up app, "tuner". Nothing
here goes to the internet.

Run by `meson test`, which says where the back end is (tests/meson.build).
"""

import hashlib
import http.server
import io
import json
import os
import platform
import subprocess
import sys
import tarfile
import tempfile
import threading
import unittest
import unittest.mock
from pathlib import Path

BACKEND_DIR = Path(os.environ["APP_STORE_BACKEND_DIR"])
sys.path.insert(0, str(BACKEND_DIR))

import app_store  # noqa: E402
import bundles  # noqa: E402
import catalog  # noqa: E402
import web  # noqa: E402

ORGANISATION = "Mavernix"
TOPIC = "mavernix-app"
EXTENSIONS = "mavernix-extensions"

EXTENSION_INDEX = """
[echo]
title = "Echo"
summary = "Says things twice"
version = "0.3.0"
icon = "echo.svg"
"""

LISTING = """
title = "Tuner"
summary = "Tunes guitars"
category = "Music"
icon = "tuner.svg"
screenshots = ["shot.png"]
command = "bin/tuner"
desktop_entry = "share/applications/tuner.desktop"
"""

DESKTOP_ENTRY = """[Desktop Entry]
Type=Application
Name=Tuner
Exec=tuner %U
TryExec=tuner
Icon=tuner

[Desktop Action Quiet]
Exec=tuner --quiet
"""


def make_bundle(files, top_folder="tuner"):
    """A .tar.gz holding `files` ({path inside the folder: text}) in top_folder/."""
    data = io.BytesIO()
    with tarfile.open(fileobj=data, mode="w:gz") as archive:
        for path, text in files.items():
            content = text.encode()
            member = tarfile.TarInfo(f"{top_folder}/{path}")
            member.size = len(content)
            member.mode = 0o755 if path.startswith("bin/") else 0o644
            archive.addfile(member, io.BytesIO(content))
    return data.getvalue()


GOOD_BUNDLE = {
    "bin/tuner": "#!/bin/sh\necho tuning\n",
    "share/applications/tuner.desktop": DESKTOP_ENTRY,
}


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    # Serves the server's folder; SimpleHTTPRequestHandler drops the ?query of a path.
    def __init__(self, request, client_address, server):
        super().__init__(request, client_address, server, directory=server.folder)

    def log_message(self, *arguments):
        self.server.requests.append(self.requestline)


class AppStoreTestCase(unittest.TestCase):
    """Sets up a GitHub of its own (self.publish) and a home of its own."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.folder = Path(self.temporary.name)
        self.site = self.folder / "site"
        self.site.mkdir()

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), QuietHandler)
        self.server.folder = str(self.site)
        self.requests = self.server.requests = []
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.address = f"http://127.0.0.1:{self.server.server_address[1]}"

        self.saved_environment = dict(os.environ)
        os.environ["MAVERNIX_APP_STORE_API"] = f"{self.address}/api"
        os.environ["XDG_CACHE_HOME"] = str(self.folder / "cache")
        os.environ["XDG_DATA_HOME"] = str(self.folder / "data")
        os.environ["XDG_CONFIG_HOME"] = str(self.folder / "config")
        # A desktop of its own, which only notes what the store tells it.
        desktop_bin = self.folder / "bin"
        desktop_bin.mkdir()
        self.told_desktop = self.folder / "told-desktop"
        command = desktop_bin / bundles.DESKTOP_COMMAND
        command.write_text(f'#!/bin/sh\necho "$*" >> {self.told_desktop}\n')
        command.chmod(0o755)
        os.environ["PATH"] = f"{desktop_bin}:{os.environ['PATH']}"
        os.environ["DESKTOP_SHELL_SETTINGS"] = str(self.folder / "shell.json")
        self.repositories = []

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        os.environ.clear()
        os.environ.update(self.saved_environment)
        self.temporary.cleanup()

    def put(self, path, content):
        file = self.site / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(content if isinstance(content, bytes) else content.encode())

    def publish(self, name="tuner", topics=(TOPIC,), version="1.0.0", listing=LISTING, bundle=None,
                checksum=None, release=True):
        """Makes `name` a repository of the organisation, with a release holding the listing, its
        pictures and the bundle. `checksum` overrides the bundle's (None: the right one)."""
        self.repositories = [repository for repository in self.repositories if repository["name"] != name]
        self.repositories.append({
            "name": name,
            "full_name": f"{ORGANISATION}/{name}",
            "html_url": f"https://github.com/{ORGANISATION}/{name}",
            "owner": {"login": ORGANISATION},
            "topics": list(topics),
            "archived": False,
        })
        self.put(f"api/orgs/{ORGANISATION}/repos", json.dumps(self.repositories))
        if not release:
            return

        bundle = bundle if bundle is not None else make_bundle(GOOD_BUNDLE)
        files = {
            catalog.LISTING_FILE: listing,
            "tuner.svg": "<svg/>",
            "shot.png": "picture",
            catalog.bundle_name(name): bundle,
        }
        assets = []
        for file_name, content in files.items():
            content = content if isinstance(content, bytes) else content.encode()
            self.put(f"downloads/{name}/{version}/{file_name}", content)
            assets.append({
                "name": file_name,
                "size": len(content),
                "browser_download_url": f"{self.address}/downloads/{name}/{version}/{file_name}",
                "digest": "sha256:" + hashlib.sha256(content).hexdigest(),
            })
        if checksum is not None:
            assets[-1]["digest"] = checksum
        self.put(f"api/repos/{ORGANISATION}/{name}/releases/latest", json.dumps({
            "tag_name": f"v{version}",
            "published_at": "2026-10-29T12:00:00Z",
            "body": "Tunes faster.",
            "assets": assets,
        }))

    def list_extensions_repository(self):
        """Makes the extensions repository one of the organisation's, with no release."""
        self.repositories = [r for r in self.repositories if r["name"] != EXTENSIONS]
        self.repositories.append({
            "name": EXTENSIONS, "full_name": f"{ORGANISATION}/{EXTENSIONS}", "topics": [], "archived": False,
            "html_url": f"https://github.com/{ORGANISATION}/{EXTENSIONS}", "owner": {"login": ORGANISATION},
        })
        self.put(f"api/orgs/{ORGANISATION}/repos", json.dumps(self.repositories))

    def publish_extensions(self, index=EXTENSION_INDEX, version="0.3.0", bundle=None, checksum=None):
        """Gives the organisation an extensions repository whose release lists `index`
        and brings the extension "echo". `checksum` overrides the bundle's."""
        self.list_extensions_repository()
        bundle = bundle if bundle is not None else make_bundle(
            {"extension.toml": 'title = "Echo"\nversion = "0.3.0"\n', "Background.qml": "Scope {}"}, top_folder="echo")
        files = {catalog.EXTENSION_INDEX_FILE: index, "echo.svg": "<svg/>", "echo.tar.gz": bundle}
        assets = []
        for file_name, content in files.items():
            content = content if isinstance(content, bytes) else content.encode()
            self.put(f"downloads/{EXTENSIONS}/{version}/{file_name}", content)
            assets.append({
                "name": file_name,
                "size": len(content),
                "browser_download_url": f"{self.address}/downloads/{EXTENSIONS}/{version}/{file_name}",
                "digest": "sha256:" + hashlib.sha256(content).hexdigest(),
            })
        if checksum is not None:
            assets[-1]["digest"] = checksum
        self.put(f"api/repos/{ORGANISATION}/{EXTENSIONS}/releases/latest", json.dumps({
            "tag_name": f"v{version}", "published_at": "2026-10-30T12:00:00Z", "body": "More echo.", "assets": assets,
        }))

    def downloads(self):
        """What is in the cache's folder of downloads."""
        folder = Path(os.environ["XDG_CACHE_HOME"]) / "mavernix" / "app-store" / "downloads"
        return list(folder.iterdir()) if folder.is_dir() else []

    def find_extension(self, name="echo"):
        return {item["name"]: item for item in catalog.find_extensions(ORGANISATION, EXTENSIONS)}[name]

    def extension_folder(self, name="echo"):
        return Path(os.environ["XDG_CONFIG_HOME"]) / "mavernix" / "extensions" / name

    def find(self, name="tuner"):
        return {app["name"]: app for app in catalog.find_apps(ORGANISATION, TOPIC)}[name]

    def install(self, name="tuner"):
        steps = []
        bundles.install(self.find(name), lambda step, fraction: steps.append((step, fraction)))
        return steps

    def app_folder(self, name="tuner"):
        return Path(os.environ["XDG_DATA_HOME"]) / "mavernix" / "apps" / name

    def entry(self, name="tuner"):
        return Path(os.environ["XDG_DATA_HOME"]) / "applications" / f"{name}.desktop"


class TestCatalog(AppStoreTestCase):
    def test_lists_repositories_with_the_topic(self):
        self.publish()
        self.publish(name="website", topics=["homepage"])
        apps = catalog.find_apps(ORGANISATION, TOPIC)
        self.assertEqual([app["name"] for app in apps], ["tuner"])
        tuner = apps[0]
        self.assertEqual(tuner["title"], "Tuner")
        self.assertEqual(tuner["version"], "1.0.0")
        self.assertEqual(tuner["whats_new"], "Tunes faster.")
        self.assertEqual(tuner["released"], "2026-10-29")
        self.assertTrue(Path(tuner["icon"]).is_file())
        self.assertEqual(len(tuner["screenshots"]), 1)

    def test_an_app_without_an_icon_is_listed_with_none(self):
        self.publish(listing=LISTING.replace('icon = "tuner.svg"', ""))
        self.publish(name="mistuned", listing=LISTING.replace("tuner.svg", "missing.svg"))
        apps = {app["name"]: app for app in catalog.find_apps(ORGANISATION, TOPIC)}
        self.assertEqual((apps["tuner"]["icon"], apps["mistuned"]["icon"]), ("", ""))

    def test_leaves_out_an_app_without_a_release(self):
        self.publish()
        self.publish(name="unfinished", release=False)
        self.assertEqual([app["name"] for app in catalog.find_apps(ORGANISATION, TOPIC)], ["tuner"])

    def test_an_unchanged_answer_comes_from_the_cache(self):
        class TaggedHandler(QuietHandler):
            asked = []

            def do_GET(self):
                TaggedHandler.asked.append(self.headers.get("If-None-Match"))
                if self.headers.get("If-None-Match") == '"one"':
                    self.send_response(304)
                    self.end_headers()
                    return
                body = b'{"answer": 42}'
                self.send_response(200)
                self.send_header("ETag", '"one"')
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        self.server.RequestHandlerClass = TaggedHandler
        self.assertEqual(web.get_json("anything"), {"answer": 42})
        self.assertEqual(web.get_json("anything"), {"answer": 42})
        self.assertEqual(TaggedHandler.asked, [None, '"one"'])


class TestListing(unittest.TestCase):
    def test_a_missing_key_is_named(self):
        with self.assertRaisesRegex(web.StoreProblem, "needs a `command`"):
            catalog.read_listing(LISTING.replace('command = "bin/tuner"', ""), "tuner's listing")

    def test_paths_must_stay_inside_the_bundle(self):
        for path in ("/usr/bin/tuner", "../../bin/tuner"):
            with self.assertRaisesRegex(web.StoreProblem, "inside the bundle"):
                catalog.read_listing(LISTING.replace('"bin/tuner"', f'"{path}"'), "tuner's listing")


class TestInstalling(AppStoreTestCase):
    def test_install_unpacks_the_app_and_adds_its_desktop_entry(self):
        self.publish()
        steps = self.install()

        self.assertEqual(steps[-1], ("installing", 1.0))
        self.assertIn(("downloading", 1.0), steps)
        program = self.app_folder() / "bundle" / "bin" / "tuner"
        self.assertTrue(os.access(program, os.X_OK))
        self.assertEqual(bundles.installed_apps()["tuner"]["version"], "1.0.0")
        self.assertEqual(bundles.installed_command("tuner"), str(program))

        entry = self.entry().read_text().splitlines()
        self.assertIn(f"Exec={program} %U", entry)
        self.assertIn(f"Icon={self.app_folder() / 'icon.svg'}", entry)
        self.assertIn("X-Mavernix-App-Store=tuner", entry)
        self.assertNotIn("TryExec=tuner", entry)
        # Only the main group changes; other groups keep their own lines.
        self.assertIn("Exec=tuner --quiet", entry)

    def test_an_update_replaces_the_old_version(self):
        self.publish(version="1.0.0")
        self.install()
        self.publish(version="1.1.0")
        self.install()
        self.assertEqual(bundles.installed_apps()["tuner"]["version"], "1.1.0")
        self.assertEqual(sorted(path.name for path in self.app_folder().parent.iterdir()), ["tuner"])

    def test_a_wrong_checksum_installs_nothing(self):
        self.publish(checksum="sha256:" + "0" * 64)
        with self.assertRaisesRegex(web.StoreProblem, "didn't arrive intact"):
            self.install()
        self.assertEqual(bundles.installed_apps(), {})
        self.assertFalse(self.entry().exists())

    def test_a_failed_update_keeps_the_old_version(self):
        self.publish(version="1.0.0")
        self.install()
        self.publish(version="2.0.0", bundle=make_bundle({"bin/tuner": "#!/bin/sh\n"}))
        with self.assertRaisesRegex(web.StoreProblem, "has no share/applications/tuner.desktop"):
            self.install()
        self.assertEqual(bundles.installed_apps()["tuner"]["version"], "1.0.0")
        self.assertTrue(self.entry().is_file())

    def test_files_outside_the_app_folder_are_refused(self):
        self.publish(bundle=make_bundle({"../../escaped": "gotcha", **GOOD_BUNDLE}))
        with self.assertRaisesRegex(web.StoreProblem, "won't unpack"):
            self.install()
        self.assertFalse((Path(os.environ["XDG_DATA_HOME"]) / "mavernix" / "escaped").exists())
        self.assertEqual(bundles.installed_apps(), {})

    def test_a_bundle_in_the_wrong_folder_is_refused(self):
        self.publish(bundle=make_bundle(GOOD_BUNDLE, top_folder="something-else"))
        with self.assertRaisesRegex(web.StoreProblem, "should hold one folder, tuner"):
            self.install()

    def test_remove_takes_the_app_and_its_entry(self):
        self.publish()
        self.install()
        bundles.remove("tuner")
        self.assertFalse(self.app_folder().exists())
        self.assertFalse(self.entry().exists())

    def test_remove_keeps_an_entry_the_user_made(self):
        self.publish()
        self.install()
        self.entry().write_text("[Desktop Entry]\nName=My own tuner\nExec=tuner\n")
        bundles.remove("tuner")
        self.assertTrue(self.entry().is_file())

    def test_an_app_without_an_icon_keeps_its_entrys_own(self):
        self.publish(listing=LISTING.replace('icon = "tuner.svg"', ""))
        self.install()
        self.assertIn("Icon=tuner", self.entry().read_text().splitlines())
        self.assertFalse(any(path.name.startswith("icon") for path in self.app_folder().iterdir()))
        self.assertEqual(bundles.installed_icon(bundles.installed_apps()["tuner"]["listing"]), "")


class TestExtensions(AppStoreTestCase):
    def install_extension(self, name="echo"):
        bundles.install(self.find_extension(name), lambda step, fraction: None)

    def test_lists_what_the_release_lists(self):
        self.publish_extensions()
        echo = self.find_extension()
        self.assertEqual((echo["kind"], echo["title"], echo["version"]), ("extension", "Echo", "0.3.0"))
        self.assertEqual(echo["whats_new"], "More echo.")
        self.assertTrue(Path(echo["icon"]).is_file())

    def test_an_extension_without_an_icon_is_listed_and_installs(self):
        self.publish_extensions(index=EXTENSION_INDEX.replace('icon = "echo.svg"', ""))
        echo = self.find_extension()
        self.assertEqual(echo["icon"], "")
        bundles.install(echo, lambda step, fraction: None)
        self.assertTrue((self.extension_folder() / "extension.toml").is_file())

    def test_leaves_out_an_entry_without_its_download_or_a_summary(self):
        self.publish_extensions(index=EXTENSION_INDEX + '''
[silent]
title = "Silent"
summary = "No download"
version = "1.0.0"
icon = "echo.svg"

[nameless]
title = "Nameless"
version = "1.0.0"
icon = "echo.svg"
''')
        self.assertEqual([item["name"] for item in catalog.find_extensions(ORGANISATION, EXTENSIONS)], ["echo"])

    def test_leaves_out_a_name_or_version_that_would_leave_the_folder(self):
        self.publish_extensions(index=EXTENSION_INDEX + '''
["../../x"]
title = "Far"
summary = "Climbs out"
version = "1.0.0"
icon = "echo.svg"

[".hidden"]
title = "Hidden"
summary = "Starts with a dot"
version = "1.0.0"
icon = "echo.svg"

[slashed]
title = "Slashed"
summary = "Version with a slash"
version = "1/../2"
icon = "echo.svg"
''')
        self.assertEqual(list(catalog.read_extension_index(
            (self.site / "downloads" / EXTENSIONS / "0.3.0" / catalog.EXTENSION_INDEX_FILE).read_text(), "index")), ["echo"])

    def test_install_puts_the_folder_where_the_desktop_looks(self):
        self.publish_extensions()
        self.install_extension()
        folder = self.extension_folder()
        self.assertTrue((folder / "extension.toml").is_file())
        self.assertEqual(bundles.installed_extensions()["echo"]["version"], "0.3.0")
        self.assertEqual([item.name for item in folder.parent.iterdir()], ["echo"])

    def test_the_download_is_not_kept(self):
        self.publish_extensions()
        self.install_extension()
        self.assertEqual(self.downloads(), [])

    def test_a_tag_with_a_slash_stays_one_folder_in_the_cache(self):
        self.publish_extensions(version="rel/1")
        self.find_extension()
        cached = Path(os.environ["XDG_CACHE_HOME"]) / "mavernix" / "app-store" / "extensions"
        self.assertEqual([folder.name for folder in cached.iterdir()], ["vrel_1"])

    def test_a_manifest_that_is_not_toml_installs_nothing(self):
        self.publish_extensions(bundle=make_bundle({"extension.toml": "title = = broken"}, top_folder="echo"))
        with self.assertRaisesRegex(web.StoreProblem, "extension.toml isn't valid"):
            self.install_extension()
        self.assertEqual(list(self.extension_folder().parent.iterdir()), [])
        self.assertEqual(self.downloads(), [])

    def test_an_update_replaces_the_folder(self):
        self.publish_extensions()
        self.install_extension()
        self.publish_extensions(version="0.4.0", bundle=make_bundle({"extension.toml": "title = 'New'\n"}, top_folder="echo"))
        self.install_extension()
        self.assertEqual((self.extension_folder() / "extension.toml").read_text(), "title = 'New'\n")
        self.assertFalse((self.extension_folder() / "Background.qml").exists())

    def test_an_extension_of_the_users_own_is_left_alone(self):
        self.publish_extensions()
        own = self.extension_folder()
        own.mkdir(parents=True)
        (own / "extension.toml").write_text("title = 'Mine'\n")
        with self.assertRaisesRegex(web.StoreProblem, "of your own"):
            self.install_extension()
        self.assertEqual((own / "extension.toml").read_text(), "title = 'Mine'\n")
        self.assertEqual(self.downloads(), [])
        self.assertFalse([request for request in self.requests if "echo.tar.gz" in request])
        self.assertEqual(bundles.installed_extensions(), {})

    def test_a_download_without_extension_toml_installs_nothing(self):
        self.publish_extensions(bundle=make_bundle({"Background.qml": "Scope {}"}, top_folder="echo"))
        with self.assertRaisesRegex(web.StoreProblem, "no extension.toml"):
            self.install_extension()
        self.assertEqual(list(self.extension_folder().parent.iterdir()), [])

    def test_a_wrong_checksum_installs_nothing(self):
        self.publish_extensions(checksum="sha256:" + "0" * 64)
        with self.assertRaisesRegex(web.StoreProblem, "didn't arrive intact"):
            self.install_extension()
        self.assertFalse(self.extension_folder().exists())

    def test_files_outside_the_folder_are_refused(self):
        self.publish_extensions(bundle=make_bundle({"../../escaped": "gotcha", "extension.toml": ""}, top_folder="echo"))
        with self.assertRaisesRegex(web.StoreProblem, "won't unpack"):
            self.install_extension()
        self.assertFalse((self.extension_folder().parent / "escaped").exists())

    def told(self):
        """What the desktop was told, one line for each time."""
        return self.told_desktop.read_text().splitlines() if self.told_desktop.exists() else []

    def test_the_desktop_switches_a_new_extension_on_and_keeps_an_updated_one_as_it_was(self):
        self.publish_extensions()
        self.install_extension()
        self.assertEqual(self.told(), ["--enable echo"])
        self.publish_extensions(version="0.4.0")
        self.install_extension()
        self.assertEqual(self.told(), ["--enable echo", ""])

    def test_the_desktop_is_told_of_a_removal(self):
        self.publish_extensions()
        self.install_extension()
        bundles.remove("echo")
        self.assertEqual(self.told(), ["--enable echo", ""])

    def test_without_a_desktop_the_extension_still_installs(self):
        del os.environ["DESKTOP_SHELL_SETTINGS"]
        self.publish_extensions()
        self.install_extension()
        self.assertTrue((self.extension_folder() / "extension.toml").is_file())
        self.assertEqual(self.told(), [])

    def test_a_desktop_that_fails_doesnt_fail_the_install(self):
        (self.folder / "bin" / bundles.DESKTOP_COMMAND).write_text("#!/bin/sh\nexit 1\n")
        self.publish_extensions()
        self.install_extension()
        self.assertEqual(bundles.installed_extensions()["echo"]["version"], "0.3.0")

    def test_a_desktop_that_hangs_doesnt_hold_up_the_install(self):
        (self.folder / "bin" / bundles.DESKTOP_COMMAND).write_text("#!/bin/sh\nsleep 5\n")
        self.publish_extensions()
        with unittest.mock.patch.object(bundles, "DESKTOP_TIMEOUT_SECONDS", 0.2):
            self.install_extension()
        self.assertEqual(bundles.installed_extensions()["echo"]["version"], "0.3.0")

    def test_remove_takes_the_folder_but_never_one_the_user_made(self):
        self.publish_extensions()
        self.install_extension()
        bundles.remove("echo")
        self.assertFalse(self.extension_folder().exists())

        own = self.extension_folder("mine")
        own.mkdir(parents=True)
        (own / "extension.toml").write_text("")
        with self.assertRaises(web.StoreProblem):
            bundles.remove("mine")
        self.assertTrue(own.is_dir())


class TestWhatIsKept(AppStoreTestCase):
    def test_a_record_that_cant_be_read_is_left_out_not_fatal(self):
        self.publish_extensions()
        bundles.install(self.find_extension(), lambda step, fraction: None)
        (self.extension_folder() / bundles.EXTENSION_RECORD).write_text("{ cut off")
        self.assertEqual(bundles.installed_extensions(), {})

    def test_an_app_and_an_extension_of_one_name_dont_borrow_each_others_records(self):
        self.publish_extensions()
        bundles.install(self.find_extension(), lambda step, fraction: None)
        self.publish(name="echo", listing=LISTING.replace("Tuner", "Echo app"))
        offered, problem = app_store.find_everything(ORGANISATION, TOPIC, EXTENSIONS)
        echoes = [app for app in app_store.apps_with_installed_versions(offered) if app["name"] == "echo"]
        self.assertEqual([(app["kind"], app["installed_version"]) for app in echoes], [("app", "")])

    def test_a_file_name_made_of_a_tag_stays_one_name(self):
        self.assertEqual(web.safe_file_name("v1.2/../3"), "v1.2_.._3")
        self.assertEqual(web.safe_file_name(".."), "_.")


class TestCommandLine(AppStoreTestCase):
    def run_back_end(self, *arguments):
        finished = subprocess.run(
            [sys.executable, str(BACKEND_DIR / "app_store.py"), *arguments],
            capture_output=True, text=True, env=os.environ,
        )
        return finished.returncode, [json.loads(line) for line in finished.stdout.splitlines()]

    def test_catalog_says_what_is_installed(self):
        self.publish()
        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC)
        self.assertEqual(code, 0)
        self.assertEqual(events[0]["apps"][0]["installed_version"], "")

        code, events = self.run_back_end("install", ORGANISATION, TOPIC, "tuner")
        self.assertEqual((code, events[-1]), (0, {"event": "done"}))
        self.assertIn("progress", {event["event"] for event in events})

        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC)
        self.assertEqual(events[0]["apps"][0]["installed_version"], "1.0.0")

    def test_apps_and_extensions_are_offered_together(self):
        self.publish()
        self.publish_extensions()
        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC, EXTENSIONS)
        self.assertEqual(code, 0)
        self.assertEqual([(app["name"], app["kind"]) for app in events[0]["apps"]],
                         [("echo", "extension"), ("tuner", "app")])

        code, events = self.run_back_end("install", ORGANISATION, TOPIC, "echo", EXTENSIONS)
        self.assertEqual((code, events[-1]), (0, {"event": "done"}))
        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC, EXTENSIONS)
        echo = events[0]["apps"][0]
        self.assertEqual((echo["installed_version"], echo["installed_command"]), ("0.3.0", ""))
        code, events = self.run_back_end("remove", "echo")
        self.assertEqual((code, events[-1]), (0, {"event": "done"}))

    def test_a_release_the_store_cant_read_is_a_problem_but_the_apps_show(self):
        self.publish()
        self.put(f"api/repos/{ORGANISATION}/{EXTENSIONS}/releases/latest", json.dumps(
            {"tag_name": "v1", "assets": []}))
        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC, EXTENSIONS)
        self.assertEqual(code, 0)
        self.assertIn("The extensions couldn't be read", events[0]["problem"])
        self.assertEqual([app["name"] for app in events[0]["apps"]], ["tuner"])

        code, events = self.run_back_end("install", ORGANISATION, TOPIC, "echo", EXTENSIONS)
        self.assertEqual(code, 1)
        self.assertIn("The extensions couldn't be read", events[-1]["message"])

    def test_a_repository_that_is_not_there_is_a_problem_but_the_apps_show(self):
        self.publish()
        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC, EXTENSIONS)
        self.assertEqual(code, 0)
        self.assertIn(f"has no repository called {EXTENSIONS}", events[0]["problem"])
        self.assertEqual([app["name"] for app in events[0]["apps"]], ["tuner"])

    def test_the_apps_show_when_the_extensions_repository_has_no_release(self):
        self.publish()
        self.list_extensions_repository()
        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC, EXTENSIONS)
        self.assertEqual((code, events[0]["problem"], [app["name"] for app in events[0]["apps"]]), (0, "", ["tuner"]))

    def test_without_github_the_installed_apps_still_show(self):
        self.publish()
        self.run_back_end("install", ORGANISATION, TOPIC, "tuner")
        os.environ["MAVERNIX_APP_STORE_API"] = "http://127.0.0.1:1/api"
        code, events = self.run_back_end("catalog", ORGANISATION, TOPIC)
        self.assertEqual(code, 0)
        self.assertIn("Couldn't reach", events[0]["problem"])
        self.assertEqual([(app["name"], app["installed_version"]) for app in events[0]["apps"]], [("tuner", "1.0.0")])

    def test_a_problem_is_reported_in_words(self):
        self.publish()
        code, events = self.run_back_end("install", ORGANISATION, TOPIC, "drums")
        self.assertEqual(code, 1)
        self.assertEqual(events, [{"event": "failed", "message": "drums isn't offered in the App Store (anymore)."}])


if __name__ == "__main__":
    # The bundle name depends on the machine; the made-up release has this machine's.
    assert catalog.bundle_name("tuner") == f"tuner-{platform.machine()}.tar.gz"
    unittest.main()
