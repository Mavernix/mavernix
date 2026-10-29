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
from pathlib import Path

BACKEND_DIR = Path(os.environ["APP_STORE_BACKEND_DIR"])
sys.path.insert(0, str(BACKEND_DIR))

import bundles  # noqa: E402
import catalog  # noqa: E402
import web  # noqa: E402

ORGANISATION = "Mavernix"
TOPIC = "mavernix-app"

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
        pass


class AppStoreTestCase(unittest.TestCase):
    """Sets up a GitHub of its own (self.publish) and a home of its own."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.folder = Path(self.temporary.name)
        self.site = self.folder / "site"
        self.site.mkdir()

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), QuietHandler)
        self.server.folder = str(self.site)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.address = f"http://127.0.0.1:{self.server.server_address[1]}"

        self.saved_environment = dict(os.environ)
        os.environ["MAVERNIX_APP_STORE_API"] = f"{self.address}/api"
        os.environ["XDG_CACHE_HOME"] = str(self.folder / "cache")
        os.environ["XDG_DATA_HOME"] = str(self.folder / "data")
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
