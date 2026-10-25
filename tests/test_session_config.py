"""The session's decisions at login (src/session): for each shipped desktop
config, the compositor's settings and the shell's are written without error and
name the right theme; the desktop that starts when none is named is the right
one; and themes are held to the format and may build on one another.

Run by `meson test`, which says where the sources are (tests/meson.build).
"""

import configparser
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SESSION_SOURCE_DIR = Path(os.environ["SESSION_SOURCE_DIR"])
CONFIG_DIR = Path(os.environ["CONFIG_SOURCE_DIR"])
THEMES_DIR = Path(os.environ["THEMES_SOURCE_DIR"])
THEME_DOCS_DIR = Path(os.environ["THEME_DOCS_DIR"])
LOOKS_DIR = Path(os.environ["LOOKS_SOURCE_DIR"])

# The desktops whose themes are shipped.
DESKTOPS = ["plain"]


class SessionTestCase(unittest.TestCase):
    """Runs the session's scripts as if the desktop were installed, from the
    sources: a paths module of its own points at the shipped configs and
    themes, and a home of its own keeps the user's files out of it."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        folder = Path(self.folder.name)

        # The installed data folder, made of the sources' configs and themes.
        data = folder / "share" / "mavernix"
        data.mkdir(parents=True)
        (data / "config").symlink_to(CONFIG_DIR)
        (data / "themes").symlink_to(THEMES_DIR)
        subprocess.run([sys.executable, str(SESSION_SOURCE_DIR / "check_theme.py"), "--collect",
                        str(THEME_DOCS_DIR), str(data / "theme-keys.json")], check=True)
        paths = (SESSION_SOURCE_DIR / "paths.py.in").read_text()
        paths = paths.replace("@LIBRARY_DIR@", str(folder / "lib" / "mavernix"))
        paths = paths.replace("@DATA_DIR@", str(data))
        (folder / "paths.py").write_text(paths)

        self.home = folder / "home"
        self.environment = {
            **os.environ,
            "HOME": str(self.home),
            "XDG_CONFIG_HOME": str(self.home / ".config"),
            "XDG_DATA_HOME": str(self.home / ".local" / "share"),
            "PYTHONPATH": str(folder),
        }

    def tearDown(self):
        self.folder.cleanup()

    def run_script(self, script, *arguments, may_fail=False):
        result = subprocess.run([sys.executable, str(SESSION_SOURCE_DIR / script), *map(str, arguments)],
                                env=self.environment, capture_output=True, text=True)
        if may_fail:
            return result
        self.assertEqual(result.returncode, 0, f"{script} failed:\n{result.stderr}")
        return result.stdout

    def user_theme(self, name, text):
        """A theme of the user's own, with theme.toml holding `text`."""
        folder = self.home / ".local" / "share" / "mavernix" / "themes" / name
        folder.mkdir(parents=True)
        (folder / "theme.toml").write_text(text)
        return folder


class TestWrittenSettings(SessionTestCase):
    def test_compositor_settings(self):
        for desktop in DESKTOPS:
            with self.subTest(desktop=desktop):
                written = self.home / f"{desktop}.ini"
                self.run_script("make_wayfire_config.py", CONFIG_DIR / f"{desktop}.toml", written)
                settings = configparser.ConfigParser(interpolation=None, strict=False)
                settings.read(written)
                self.assertIn("themed-decoration", settings["core"]["plugins"].split())
                self.assertEqual(Path(settings["themed-decoration"]["theme"]).name, desktop)

    def test_shell_settings(self):
        for desktop in DESKTOPS:
            with self.subTest(desktop=desktop):
                written = self.home / f"{desktop}.json"
                self.run_script("make_shell_config.py", CONFIG_DIR / f"{desktop}.toml", written)
                settings = json.loads(written.read_text())
                self.assertEqual(settings["desktop"], desktop)
                self.assertEqual(Path(settings["theme_directory"]).name, desktop)
                self.assertIn("window", settings["theme"])


class TestPickedDesktop(SessionTestCase):
    def picked(self):
        return self.run_script("pick_desktop.py").strip()

    def choose(self, name):
        chosen = self.home / ".config" / "mavernix" / "desktop"
        chosen.parent.mkdir(parents=True, exist_ok=True)
        chosen.write_text(name + "\n")

    def import_sea_lion(self):
        """What the Theme Installer leaves: Sea Lion's themes and its desktops.
        Any theme will do as the imported one; the plain theme is at hand."""
        data = self.home / ".local" / "share" / "mavernix"
        for theme in ("sea-lion-dark", "sea-lion"):
            shutil.copytree(THEMES_DIR / "plain", data / "themes" / theme)
        (data / "config").mkdir(parents=True)
        for desktop in ("sea-lion-dark", "sea-lion"):
            shutil.copyfile(LOOKS_DIR / "sea-lion" / f"{desktop}.toml", data / "config" / f"{desktop}.toml")

    def test_plain_before_anything_is_imported(self):
        self.assertEqual(self.picked(), "plain")

    def test_sea_lion_once_its_theme_is_imported(self):
        self.import_sea_lion()
        self.assertEqual(self.picked(), "sea-lion")

    def test_the_users_choice_comes_first(self):
        self.import_sea_lion()
        self.choose("plain")
        self.assertEqual(self.picked(), "plain")

    def test_a_choice_whose_theme_is_missing_is_passed_over(self):
        self.choose("sea-lion")
        self.assertEqual(self.picked(), "plain")
        self.choose("no-such-desktop")
        self.assertEqual(self.picked(), "plain")


class TestThemes(SessionTestCase):
    def check(self, theme_folder):
        return self.run_script("check_theme.py", theme_folder, may_fail=True)

    def test_plain_follows_the_format(self):
        result = self.check(THEMES_DIR / "plain")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_a_misspelt_key_is_named(self):
        folder = self.user_theme("misspelt", 'based_on = "plain"\n[menu]\nbackgrund = "#000000"\n')
        result = self.check(folder)
        self.assertEqual(result.returncode, 1)
        self.assertIn("menu.backgrund", result.stdout)

    def test_a_theme_built_on_another(self):
        folder = self.user_theme("darker", 'based_on = "plain"\n[menu]\nbackground = "#000000"\n'
                                 '[panel]\nbackground = { image = "images/panel.png" }\n')
        (folder / "images").mkdir()
        (folder / "images" / "trash-full.svg").write_text("<svg/>")
        config = self.home / "darker.toml"
        config.write_text((CONFIG_DIR / "plain.toml").read_text().replace('theme = "plain"', 'theme = "darker"'))

        written = self.home / "darker.json"
        self.run_script("make_shell_config.py", config, written)
        settings = json.loads(written.read_text())
        theme = settings["theme"]
        self.assertEqual(theme["menu"]["background"], "#000000")
        # A fill is taken whole: plain's colour does not stay beside the picture.
        self.assertEqual(theme["panel"]["background"], {"image": "images/panel.png"})
        self.assertIn("height", theme["panel"])

        joined = Path(settings["theme_directory"])
        self.assertEqual((joined / "images" / "trash-full.svg").read_text(), "<svg/>")
        self.assertTrue((joined / "images" / "app-grid.svg").is_file())

    def test_a_missing_icon_theme_is_replaced_by_an_installed_one(self):
        self.user_theme("odd-icons", 'based_on = "plain"\n[icons]\ntheme = "NoSuchIcons"\n')
        icons = self.home / "data-dir" / "icons" / "breeze"
        icons.mkdir(parents=True)
        (icons / "index.theme").write_text("[Icon Theme]\nName=Breeze\n")
        self.environment["XDG_DATA_DIRS"] = str(self.home / "data-dir")
        config = self.home / "odd-icons.toml"
        config.write_text((CONFIG_DIR / "plain.toml").read_text().replace('theme = "plain"', 'theme = "odd-icons"'))

        written = self.home / "odd-icons.json"
        self.run_script("make_shell_config.py", config, written)
        self.assertEqual(json.loads(written.read_text())["theme"]["icons"]["theme"], "breeze")


if __name__ == "__main__":
    unittest.main()
