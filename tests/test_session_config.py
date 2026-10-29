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
import tomllib
import unittest
import unittest.mock
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
        # Run from inside a desktop session, the scripts would ask its compositor about its screens.
        without_compositor = unittest.mock.patch.dict(os.environ)
        without_compositor.start()
        self.addCleanup(without_compositor.stop)
        os.environ.pop("WAYFIRE_SOCKET", None)

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


class TestScreenScale(SessionTestCase):
    def test_a_stretched_mode_is_less_dense(self):
        # A dense panel shown at half its width is drawn at once, not twice.
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        panel = {"eDP-1": {"width": 2560, "dpi": 227.0}}
        with unittest.mock.patch.object(make_wayfire_config, "connected_screens", return_value=panel):
            native = make_wayfire_config.display_sections({})
            stretched = make_wayfire_config.display_sections(
                {"display": {"screens": {"eDP-1": {"mode": "1280x800@60000"}}}})
        self.assertIn("scale = 2", native[0])
        self.assertIn("scale = 1", stretched[0])

    def test_a_size_the_screen_does_not_list_gets_its_timings(self):
        # Wayfire sets only a listed size on a real screen; any other is
        # added to the list from a modeline, here as `cvt -r 1920 1200 120`.
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        panel = {"eDP-1": {"width": 2560, "dpi": 227.0, "sizes": {"2560x1600"}}}

        def written(mode):
            with unittest.mock.patch.object(make_wayfire_config, "connected_screens", return_value=panel):
                return make_wayfire_config.display_sections({"display": {"screens": {"eDP-1": {"mode": mode}}}})[0]

        self.assertIn("custom_mode = 317.00 1920 1968 2000 2080 1200 1203 1209 1271 +hsync -vsync",
                      written("1920x1200@120000"))
        self.assertFalse(any(line.startswith("custom_mode") for line in written("2560x1600@120000")))


    def test_an_unplaced_screen_keeps_where_it_is(self):
        # With several screens, rewriting the config keeps the running
        # arrangement; the config's own position still wins.
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        screens = {"DP-1": {"width": 2560, "height": 1440, "dpi": 93.0},
                   "HDMI-A-1": {"width": 1920, "height": 1080, "dpi": 96.0}}
        current = {"DP-1": {"position": "0,0", "width": 2560}, "HDMI-A-1": {"position": "2560,300", "width": 1920}}
        with unittest.mock.patch.object(make_wayfire_config, "connected_screens", return_value=screens), \
                unittest.mock.patch.object(make_wayfire_config, "screen_positions", return_value=current):
            kept = make_wayfire_config.display_sections({})
            placed = make_wayfire_config.display_sections(
                {"display": {"screens": {"HDMI-A-1": {"position": "0,1440"}}}})
            # DP-1 at scale 2 is half as wide, and HDMI-A-1 would stand off by itself.
            rescaled = make_wayfire_config.display_sections({"display": {"screens": {"DP-1": {"scale": 2}}}})
        self.assertIn("position = 2560,300", kept[1])
        self.assertIn("position = 0,1440", placed[1])
        self.assertEqual(1, sum(line.startswith("position") for line in placed[1]))
        self.assertFalse(any(line.startswith("position") for section in rescaled for line in section))

    def test_placed_screens_touch_again_after_a_change_of_size(self):
        # DP-1 at scale 2 is half as wide: HDMI-A-1, placed beside it at its
        # old width, is moved back to its edge instead of standing apart.
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        screens = {"DP-1": {"width": 2560, "height": 1440, "dpi": 93.0},
                   "HDMI-A-1": {"width": 1920, "height": 1080, "dpi": 96.0}}
        placed = {"DP-1": {"position": "0,0"}, "HDMI-A-1": {"position": "2560,0"}}
        with unittest.mock.patch.object(make_wayfire_config, "connected_screens", return_value=screens), \
                unittest.mock.patch.object(make_wayfire_config, "screen_positions", return_value={}):
            same = make_wayfire_config.display_sections({"display": {"screens": placed}})
            placed["DP-1"]["scale"] = 2
            rescaled = make_wayfire_config.display_sections({"display": {"screens": placed}})
        self.assertIn("position = 2560,0", same[1])
        self.assertIn("position = 0,0", rescaled[0])
        self.assertIn("position = 1280,0", rescaled[1])


class TestRefreshDesktops(SessionTestCase):
    def test_an_imported_desktop_is_brought_up_to_date(self):
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import refresh_desktops
        finally:
            del sys.path[:2]
        folder = Path(self.folder.name)
        looks = folder / "looks"
        (looks / "lion").mkdir(parents=True)
        (looks / "lion" / "look.toml").write_text('desktops = ["lion", "lion-dark"]\n')
        (looks / "lion" / "lion.toml").write_text('theme = "lion"\nnew_word = "Zoom"\n')
        (looks / "lion" / "lion-dark.toml").write_text('theme = "lion-dark"\n')
        (looks / "other").mkdir()
        (looks / "other" / "look.toml").write_text('desktops = ["other"]\n')
        (looks / "other" / "other.toml").write_text('theme = "other"\n')
        configs = folder / "configs"
        configs.mkdir()
        # Imported before records were kept, and up to date: one stale, one not.
        (configs / "lion.toml").write_text('theme = "lion"\n')
        (configs / "lion-dark.toml").write_text('theme = "lion-dark"\n')

        self.assertEqual(refresh_desktops.refresh(looks, configs), ["lion"])
        self.assertIn("new_word", (configs / "lion.toml").read_text())
        # Without a record it may have been changed by hand, so it is kept.
        self.assertEqual((configs / "lion.toml.before-update").read_text(), 'theme = "lion"\n')
        # A look never imported is not added.
        self.assertFalse((configs / "other.toml").exists())

        # Next time, a copy as it was installed is replaced without a spare.
        (configs / "lion.toml.before-update").unlink()
        (looks / "lion" / "lion.toml").write_text('theme = "lion"\nnewer_word = "Zoom"\n')
        self.assertEqual(refresh_desktops.refresh(looks, configs), ["lion"])
        self.assertFalse((configs / "lion.toml.before-update").exists())
        self.assertEqual(refresh_desktops.refresh(looks, configs), [])


class TestRefreshThemes(SessionTestCase):
    def test_an_imported_theme_gets_the_settings_it_lacks(self):
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import refresh_themes
        finally:
            del sys.path[:2]
        folder = Path(self.folder.name)
        looks = folder / "looks"
        (looks / "lion").mkdir(parents=True)
        (looks / "lion" / "look.toml").write_text('themes = ["lion", "lion-new"]\n')
        (looks / "lion" / "updates.toml").write_text(
            '[[update]]\nrevision = 1\n[update.settings."dock.grid"]\nshare = 0.8\nspacing = 28\n'
            '[[update]]\nrevision = 2\n[update.settings.fan]\nmost = 8\n')
        themes = folder / "themes"
        (themes / "lion").mkdir(parents=True)
        (themes / "lion-new").mkdir()
        # From before revisions were kept, with one of the new keys set by hand.
        old = '# made by hand\n[theme]\nname = "Lion"\n\n[dock.grid]\nspacing = 10\nwidth = 96\n'
        (themes / "lion" / "theme.toml").write_text(old)
        current = '[theme]\nname = "Lion"\nrevision = 2\n'
        (themes / "lion-new" / "theme.toml").write_text(current)

        self.assertEqual(refresh_themes.refresh(looks, themes), ["lion"])
        text = (themes / "lion" / "theme.toml").read_text()
        theme = tomllib.loads(text)
        self.assertEqual(theme["dock"]["grid"], {"share": 0.8, "spacing": 10, "width": 96})
        # A table the theme lacked is added; the file's comment stays.
        self.assertEqual(theme["fan"], {"most": 8})
        self.assertTrue(text.startswith("# made by hand\n"))
        # A theme made after the updates is left alone, and so is one up to date.
        self.assertEqual((themes / "lion-new" / "theme.toml").read_text(), current)
        self.assertEqual(refresh_themes.refresh(looks, themes), [])


class TestPointerSettings(SessionTestCase):
    def test_speed_and_acceleration_reach_the_compositor(self):
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        lines = make_wayfire_config.input_section(
            {"input": {"mouse_speed": -0.5, "mouse_acceleration": False, "touchpad_acceleration": True}}, {})
        self.assertIn("mouse_cursor_speed = -0.5", lines)
        self.assertIn("mouse_accel_profile = flat", lines)
        self.assertIn("touchpad_accel_profile = adaptive", lines)
        self.assertFalse(any(line.startswith("touchpad_cursor_speed") for line in lines))
        more = make_wayfire_config.input_section({"input": {"mouse_scroll_speed": 2.5, "left_handed": True}}, {})
        self.assertIn("mouse_scroll_speed = 2.5", more)
        self.assertIn("left_handed_mode = true", more)
        with self.assertRaises(SystemExit):
            make_wayfire_config.input_section({"input": {"mouse_speed": 2}}, {})


class TestKdeColours(SessionTestCase):
    def test_default_scheme_is_the_themes(self):
        # A KDE program set to "Default" takes Breeze's scheme by its file
        # name; the session's own copy of that name must hold the theme's.
        output = self.home / "app-theme"
        self.run_script("make_app_theme.py", THEMES_DIR / "plain", output)
        schemes = output / "share" / "color-schemes"

        def colours(path):
            scheme = configparser.ConfigParser(interpolation=None, strict=False)
            scheme.optionxform = str
            scheme.read(path)
            return {name: dict(scheme[name]) for name in scheme.sections() if name.startswith("Colors:")}

        theme = colours(schemes / "DesktopTheme.colors")
        self.assertIn("Colors:View", theme)
        for name in ("BreezeLight", "BreezeDark"):
            with self.subTest(scheme=name):
                self.assertEqual(colours(schemes / f"{name}.colors"), theme)


if __name__ == "__main__":
    unittest.main()
