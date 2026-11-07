"""The session's decisions at login (src/session): for each shipped desktop
config, the compositor's settings and the shell's are written without error and
name the right theme; the desktop that starts when none is named is the right
one; and themes are held to the format and may build on one another.

Run by `meson test`, which says where the sources are (tests/meson.build).
"""

import configparser
import contextlib
import io
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

    def test_the_users_own_wayfire_settings_come_last(self):
        own = self.home / ".config" / "mavernix" / "wayfire.ini"
        own.parent.mkdir(parents=True)
        own.write_text("[workarounds]\ndisable_primary_selection = true\n")
        written = self.home / "plain.ini"
        self.run_script("make_wayfire_config.py", CONFIG_DIR / "plain.toml", written)
        text = written.read_text()
        self.assertIn("disable_primary_selection = false", text)
        self.assertTrue(text.rstrip().endswith("disable_primary_selection = true"))

    def test_the_users_own_plugins_are_added_to_the_list(self):
        own = self.home / ".config" / "mavernix" / "wayfire.ini"
        own.parent.mkdir(parents=True)
        own.write_text("[core]\nplugins = oswitch move\n\n[oswitch]\nnext_output_with_win = <shift> <super> KEY_RIGHT\n")
        written = self.home / "plain.ini"
        self.run_script("make_wayfire_config.py", CONFIG_DIR / "plain.toml", written)
        settings = configparser.ConfigParser(interpolation=None, strict=False)
        settings.read(written)
        plugins = settings["core"]["plugins"].split()
        self.assertIn("themed-decoration", plugins)
        self.assertIn("oswitch", plugins)
        self.assertEqual(plugins.count("move"), 1)
        self.assertEqual(written.read_text().count("plugins ="), 1)
        self.assertEqual(settings["oswitch"]["next_output_with_win"], "<shift> <super> KEY_RIGHT")

    def test_closed_programs_are_noted_unless_switched_off(self):
        for note in (True, False):
            with self.subTest(note=note):
                text = (CONFIG_DIR / "plain.toml").read_text()
                self.home.mkdir(parents=True, exist_ok=True)
                desktop = self.home / "plain.toml"
                desktop.write_text(text.replace("note_closed_programs = true",
                                                f"note_closed_programs = {str(note).lower()}"))
                written = self.home / "plain.ini"
                self.run_script("make_wayfire_config.py", desktop, written)
                settings = configparser.ConfigParser(interpolation=None, strict=False)
                settings.read(written)
                self.assertEqual("closed-programs" in settings["core"]["plugins"].split(), note)

    def test_shell_settings(self):
        for desktop in DESKTOPS:
            with self.subTest(desktop=desktop):
                written = self.home / f"{desktop}.json"
                self.run_script("make_shell_config.py", CONFIG_DIR / f"{desktop}.toml", written)
                settings = json.loads(written.read_text())
                self.assertEqual(settings["desktop"], desktop)
                self.assertEqual(Path(settings["theme_directory"]).name, desktop)
                self.assertIn("window", settings["theme"])

    def test_a_half_installed_extension_is_not_taken_for_one(self):
        folder = self.home / ".config" / "mavernix" / "extensions"
        for name in ("echo", ".echo-old"):
            (folder / name).mkdir(parents=True)
            (folder / name / "extension.toml").write_text('title = "Echo"\n')
        written = self.home / "plain.json"
        self.run_script("make_shell_config.py", CONFIG_DIR / "plain.toml", written)
        self.assertEqual(list(json.loads(written.read_text())["extensions"]), ["echo"])


class TestRefreshedExtensions(SessionTestCase):
    """An extension installed while the desktop runs (refresh_extensions.py)."""

    def setUp(self):
        super().setUp()
        self.settings_file = self.home / "state" / "shell.json"
        self.run_script("make_shell_config.py", CONFIG_DIR / "plain.toml", self.settings_file)
        self.settings = json.loads(self.settings_file.read_text())

        # A shell of its own, which notes what it is asked and whether the
        # list was written by then.
        shell_bin = Path(self.folder.name) / "bin"
        shell_bin.mkdir()
        self.asked_shell = Path(self.folder.name) / "asked-shell"
        shell = shell_bin / "qs"
        shell.write_text(f"""#!/bin/sh
listed=no; [ -f {self.settings["extensions_file"]} ] && listed=yes
echo "$* (list written: $listed)" >> {self.asked_shell}
""")
        shell.chmod(0o755)
        self.environment["PATH"] = f"{shell_bin}:{self.environment['PATH']}"

    def add_extension(self, name):
        folder = self.home / ".config" / "mavernix" / "extensions" / name
        folder.mkdir(parents=True)
        (folder / "extension.toml").write_text(f'title = "{name}"\n')

    def found(self):
        return list(json.loads(Path(self.settings["extensions_file"]).read_text())["extensions"])

    def asked(self):
        """What the shell was asked, one line for each time, without the shell's folder."""
        if not self.asked_shell.exists():
            return []
        return [line.split(" ipc ", 1)[1] for line in self.asked_shell.read_text().splitlines()]

    def test_the_list_beside_the_settings_follows_the_folder(self):
        self.assertEqual(Path(self.settings["extensions_file"]), self.settings_file.with_name("extensions.json"))
        self.run_script("refresh_extensions.py", self.settings_file)
        self.assertEqual(self.found(), [])
        self.add_extension("echo")
        self.run_script("refresh_extensions.py", self.settings_file)
        self.assertEqual(self.found(), ["echo"])
        self.assertEqual(self.asked(), [])

    def test_an_update_is_loaded_through_a_new_link(self):
        # The shell keeps code by its path; an update must come by a new one.
        def load_directory():
            self.run_script("refresh_extensions.py", self.settings_file)
            found = json.loads(Path(self.settings["extensions_file"]).read_text())["extensions"]
            return Path(found["echo"]["load_directory"])

        self.add_extension("echo")
        folder = self.home / ".config" / "mavernix" / "extensions" / "echo"
        first = load_directory()
        self.assertEqual(first.resolve(), folder.resolve())
        self.assertEqual(load_directory(), first)

        # As the App Store updates it: a new folder swapped in.
        staged = folder.with_name(".echo-new")
        staged.mkdir()
        (staged / "extension.toml").write_text('title = "echo"\nversion = "2"\n')
        shutil.rmtree(folder)
        staged.rename(folder)
        second = load_directory()
        self.assertNotEqual(second, first)
        self.assertEqual(second.resolve(), folder.resolve())

        # As someone working on it: a file changed in place.
        manifest = folder / "extension.toml"
        manifest.write_text(manifest.read_text() + 'description = "longer"\n')
        os.utime(manifest, ns=(1, manifest.stat().st_mtime_ns + 1))
        self.assertNotIn(load_directory(), (first, second))

    def test_enable_asks_the_shell_once_the_list_is_written(self):
        self.add_extension("echo")
        self.run_script("refresh_extensions.py", self.settings_file, "--enable", "echo")
        self.assertEqual(self.asked(), ["call extensions switchOn echo (list written: yes)"])

    def test_the_users_choices_are_left_to_the_shell(self):
        self.add_extension("echo")
        self.run_script("refresh_extensions.py", self.settings_file, "--enable", "echo")
        self.assertFalse(Path(self.settings["user_choices_file"]).exists())

    def test_a_name_not_found_still_writes_the_list(self):
        self.add_extension("echo")
        result = self.run_script("refresh_extensions.py", self.settings_file, "--enable", "nothing", may_fail=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no extension called nothing", result.stderr)
        self.assertEqual(self.found(), ["echo"])
        self.assertEqual(self.asked(), [])

    def test_a_shell_that_fails_is_reported(self):
        (Path(self.folder.name) / "bin" / "qs").write_text("#!/bin/sh\necho 'no shell running' >&2\nexit 1\n")
        self.add_extension("echo")
        result = self.run_script("refresh_extensions.py", self.settings_file, "--enable", "echo", may_fail=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("no shell running", result.stderr)
        self.assertEqual(self.found(), ["echo"])


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

    def icon_themes_picked(self, picked):
        """The shell settings' icon themes on plain, for programs and for the
        shell itself, when the user picked `picked` (Settings, App Style)."""
        for installed in ("breeze", "Papirus"):
            icons = self.home / "data-dir" / "icons" / installed
            icons.mkdir(parents=True, exist_ok=True)
            (icons / "index.theme").write_text(f"[Icon Theme]\nName={installed}\n")
        self.environment["XDG_DATA_DIRS"] = str(self.home / "data-dir")
        choices = self.home / ".config" / "mavernix" / "plain.json"
        choices.parent.mkdir(parents=True, exist_ok=True)
        choices.write_text(json.dumps({"icons": picked}))

        written = self.home / "plain.json"
        self.run_script("make_shell_config.py", CONFIG_DIR / "plain.toml", written)
        settings = json.loads(written.read_text())
        return settings["theme"]["icons"]["theme"], settings["shell_icon_theme"]

    def test_the_users_icon_theme_takes_the_themes_place(self):
        self.assertEqual(self.icon_themes_picked({"programs": "Papirus", "dock_and_menu_bar": "breeze"}),
                         ("Papirus", "breeze"))

    def test_programs_icons_leave_the_docks_alone(self):
        # plain's own is Adwaita; installed here only as the stand-in breeze.
        self.assertEqual(self.icon_themes_picked({"programs": "Papirus"}), ("Papirus", "breeze"))

    def test_a_picked_icon_theme_that_is_gone_is_replaced(self):
        self.assertEqual(self.icon_themes_picked({"programs": "Uninstalled", "dock_and_menu_bar": "Gone"}),
                         ("breeze", "breeze"))


class TestWindowAnimations(SessionTestCase):
    def test_pop_is_our_plugins_and_can_be_shortened(self):
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        theme = {"animation": {"window_open": {"kind": "pop", "duration_ms": 200, "curve": "ease-out"}}}
        sections, plugins = make_wayfire_config.animation_sections(theme)
        self.assertIn("themed-open-close", plugins)
        self.assertIn("open_animation = pop", sections[0])
        # Not X11 menus, which Wayland menus are not either.
        self.assertIn('enabled_for = type equals "toplevel"', sections[0])
        self.assertIn(["[themed-open-close]", "pop_duration = 200ms circle"], sections)

        # A desktop's [window] quick_open_close_ms keeps the curve.
        make_wayfire_config.shorten_open_and_close(sections, 120)
        self.assertIn(["[themed-open-close]", "pop_duration = 120ms circle"], sections)


class TestUncoveredColour(SessionTestCase):
    def test_pushed_desktop_uncovers_the_lists_colour(self):
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        theme = {"animation": {"notification_list": {"kind": "push"}},
                 "notifications": {"list": {"background": {"gradient": ["#000000", "#336699", "#ffffff"]}}}}
        self.assertEqual(make_wayfire_config.uncovered_color_lines(theme),
                         ["background_color = 0.200 0.400 0.600 1.000"])
        theme["notifications"]["list"]["background"] = {"color": "#ff000080"}
        self.assertEqual(make_wayfire_config.uncovered_color_lines(theme),
                         ["background_color = 1.000 0.000 0.000 0.502"])
        # A list that slides in over the desktop uncovers nothing.
        theme["animation"]["notification_list"]["kind"] = "slide"
        self.assertEqual(make_wayfire_config.uncovered_color_lines(theme), [])


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


class TestSeaLionUpdates(unittest.TestCase):
    """Sea Lion's updates.toml carries tables to themes imported before they
    existed: the App Store's and the menus' fade. Each is a copy of what an
    import writes, so it must stay the same as make_theme's, and Sea Lion
    Dark's App Store table must have the same keys."""

    def setUp(self):
        sys.path.insert(0, str(LOOKS_DIR.parent))
        self.addCleanup(sys.path.remove, str(LOOKS_DIR.parent))
        import make_theme
        self.make_theme = make_theme
        with open(LOOKS_DIR / "sea-lion" / "updates.toml", "rb") as file:
            self.updates = {update["revision"]: update for update in tomllib.load(file)["update"]}

    def test_the_light_table_is_the_imports(self):
        made = tomllib.loads(self.make_theme.APP_STORE_TABLE + self.make_theme.APP_STORE_STAND_INS)
        self.assertEqual(self.updates[2]["themes"], ["sea-lion"])
        self.assertEqual(self.updates[2]["settings"]["app_store"], made["app_store"])

    def test_the_dark_table_has_the_same_keys(self):
        self.assertEqual(self.updates[3]["themes"], ["sea-lion-dark"])
        self.assertEqual(self.updates[3]["settings"]["app_store"].keys(), self.updates[2]["settings"]["app_store"].keys())
        self.assertEqual(self.updates[3]["settings"]["fonts"], self.updates[2]["settings"]["fonts"])

    def test_older_themes_get_the_imports_menu_fade(self):
        made = tomllib.loads(self.make_theme.MENU_CLOSE_TABLE)
        self.assertNotIn("themes", self.updates[5])
        self.assertEqual(self.updates[5]["settings"]["animation.menu_close"], made["animation"]["menu_close"])

    def test_older_light_themes_get_the_imports_panel_edge(self):
        self.assertEqual(self.updates[8]["themes"], ["sea-lion"])
        [correction] = self.updates[8]["corrections"]
        self.assertEqual((correction["table"], correction["key"]), ("panel", "separator"))
        self.assertEqual(correction["now"], [self.make_theme.PANEL_EDGE])


class TestSeaLionMusic(unittest.TestCase):
    """Bandstand's [music] table names pictures make_music_pictures cuts from
    iTunes' resource files, which the import must take from the installer;
    Sea Lion Dark writes ProKit's under the same names."""

    def setUp(self):
        sys.path.insert(0, str(LOOKS_DIR.parent))
        self.addCleanup(sys.path.remove, str(LOOKS_DIR.parent))
        import installer_image
        import make_dark_theme
        import make_theme
        self.installer_image = installer_image
        self.make_dark_theme = make_dark_theme
        self.make_theme = make_theme

    def pictures_named(self, table):
        names = set()
        for value in table.values():
            if isinstance(value, dict):
                names |= self.pictures_named(value)
            elif isinstance(value, str) and value.startswith("images/music/"):
                names.add(value.removeprefix("images/music/").removesuffix(".png"))
        return names

    def test_every_named_picture_is_cut(self):
        music = tomllib.loads(self.make_theme.MUSIC_TABLE)["music"]
        self.assertLessEqual(self.pictures_named(music), set(self.make_theme.MUSIC_PICTURES))

    def test_itunes_files_are_taken_and_read_by_position(self):
        taken = self.installer_image.needed_files(LOOKS_DIR / "sea-lion")["Essentials"]
        for name in self.make_theme.ITUNES_NORMAL_FILES + self.make_theme.ITUNES_DOUBLE_FILES:
            path = f"{self.make_theme.ITUNES_RESOURCES}/{name}.rsrc"
            with self.subTest(path=path):
                self.assertIn(path, taken)
                self.assertIn(path, self.installer_image.DECODED_BY_POSITION)

    def test_the_dark_pictures_have_light_names(self):
        dark = self.make_dark_theme
        written = {name + suffix for name in dark.MUSIC_GLYPHS for suffix in dark.MUSIC_GLYPH_STATES}
        written |= set(dark.MUSIC_PICTURES) - set(dark.MUSIC_DARK_ONLY)
        # the -inactive glyphs are the dark theme's own addition
        self.assertLessEqual({name for name in written if not name.endswith("-inactive")},
                             set(self.make_theme.MUSIC_PICTURES))


class TestSeaLionIcons(unittest.TestCase):
    """make_icons.py reads only what the import takes from the installer, so
    every icon it names must be in needed-files.txt."""

    def setUp(self):
        sys.path.insert(0, str(LOOKS_DIR.parent))
        self.addCleanup(sys.path.remove, str(LOOKS_DIR.parent))
        import installer_image
        import make_icons
        self.make_icons = make_icons
        self.taken = {path for files in installer_image.needed_files(LOOKS_DIR / "sea-lion").values()
                      for path in files}

    def test_every_source_is_taken(self):
        icons = self.make_icons
        sources = [source for _, source in [*icons.ICONS.values(), *icons.SIDE_BAR_ICONS.values()]]
        for source in sources + icons.BUNDLES:
            with self.subTest(source=source):
                if source.endswith(".icns"):
                    self.assertIn(source if "/" in source else f"{icons.SYSTEM_ICONS}/{source}", self.taken)
                else:
                    # The icon's file name is in the Info.plist, which only the installer has.
                    self.assertIn(f"{source}/Contents/Info.plist", self.taken)
                    resources = f"{source}/Contents/Resources/"
                    self.assertTrue(any(path.startswith(resources) and path.endswith(".icns") for path in self.taken))


class TestThemeUpdates(unittest.TestCase):
    """The Theme Installer's updates.py: what imported themes get at login,
    and when a new import is offered."""

    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.folder)
        sys.path.insert(0, str(LOOKS_DIR.parent))
        self.addCleanup(sys.path.remove, str(LOOKS_DIR.parent))
        import updates
        self.updates = updates
        self.look = self.folder / "looks" / "lion"
        self.look.mkdir(parents=True)
        (self.look / "look.toml").write_text('themes = ["lion"]\n')
        self.themes = self.folder / "themes"
        (self.themes / "lion").mkdir(parents=True)

    def write_updates(self, text):
        (self.look / "updates.toml").write_text(text)

    def theme_text(self):
        return (self.themes / "lion" / "theme.toml").read_text()

    def test_settings_and_corrections_are_applied_once(self):
        self.write_updates('[[update]]\nrevision = 1\n'
                           '[update.settings."dock.grid"]\nshare = 0.8\n'
                           '[update.settings.fan]\nmost = 8\n'
                           '[[update.corrections]]\ntable = "dock.grid"\nkey = "width"\nwas = 96\nnow = 100\n')
        # From before revisions were kept; a comment after a header, too.
        (self.themes / "lion" / "theme.toml").write_text(
            '# made by hand\n[theme]\nname = "Lion"\n\n[dock.grid]  # the grid\nwidth = 96\n')

        self.assertEqual(self.updates.refresh(self.folder / "looks", self.themes), ["lion"])
        theme = tomllib.loads(self.theme_text())
        self.assertEqual(theme["dock"]["grid"], {"share": 0.8, "width": 100})
        self.assertEqual(theme["fan"], {"most": 8})
        self.assertEqual(theme["theme"]["updated_to"], 1)
        self.assertTrue(self.theme_text().startswith("# made by hand\n"))

        # What the user changes afterwards stays, even back to the old value.
        edited = self.theme_text().replace("width = 100", "width = 96").replace("share = 0.8\n", "")
        (self.themes / "lion" / "theme.toml").write_text(edited)
        self.assertEqual(self.updates.refresh(self.folder / "looks", self.themes), [])
        self.assertEqual(self.theme_text(), edited)

    def test_an_update_can_be_for_some_themes_only(self):
        (self.look / "look.toml").write_text('themes = ["lion", "lion-dark"]\n')
        (self.themes / "lion-dark").mkdir()
        self.write_updates('[[update]]\nrevision = 1\nthemes = ["lion"]\n[update.settings.window]\ncolor = "#ffffff"\n'
                           '[[update]]\nrevision = 2\nthemes = ["lion-dark"]\n[update.settings.window]\ncolor = "#202020"\n')
        for name in ("lion", "lion-dark"):
            (self.themes / name / "theme.toml").write_text('[theme]\nname = "Lion"\n')

        self.assertEqual(sorted(self.updates.refresh(self.folder / "looks", self.themes)), ["lion", "lion-dark"])
        light = tomllib.loads(self.theme_text())
        dark = tomllib.loads((self.themes / "lion-dark" / "theme.toml").read_text())
        self.assertEqual((light["window"]["color"], light["theme"]["updated_to"]), ("#ffffff", 2))
        self.assertEqual((dark["window"]["color"], dark["theme"]["updated_to"]), ("#202020", 2))

    def test_a_table_written_inline_is_left_alone(self):
        self.write_updates('[[update]]\nrevision = 1\n[update.settings."dock.grid"]\nshare = 0.8\n')
        old = '[theme]\nname = "Lion"\n[dock]\ngrid = { width = 96 }\n'
        (self.themes / "lion" / "theme.toml").write_text(old)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.updates.refresh(self.folder / "looks", self.themes), [])
        self.assertEqual(self.theme_text(), old)

    def test_later_holds_until_an_update_brings_more(self):
        os.environ["XDG_CONFIG_HOME"] = str(self.folder / "config")
        self.addCleanup(os.environ.pop, "XDG_CONFIG_HOME")
        look = {"themes": ["lion"], "folder": self.look}
        (self.themes / "lion" / "theme.toml").write_text('[theme]\nname = "Lion"\n')
        self.write_updates('[[update]]\nrevision = 1\nimport_brings = ["An icon."]\n')
        self.assertTrue(self.updates.offers_import("lion", look, self.themes))

        self.updates.put_off({"lion": 1})
        self.assertFalse(self.updates.offers_import("lion", look, self.themes))
        # An update with settings only does not bring the offer back.
        self.write_updates('[[update]]\nrevision = 1\nimport_brings = ["An icon."]\n'
                           '[[update]]\nrevision = 2\n[update.settings.fan]\nmost = 8\n')
        self.assertFalse(self.updates.offers_import("lion", look, self.themes))
        # One that brings something new does.
        self.write_updates('[[update]]\nrevision = 1\nimport_brings = ["An icon."]\n'
                           '[[update]]\nrevision = 2\nimport_brings = ["A button."]\n')
        self.assertTrue(self.updates.offers_import("lion", look, self.themes))


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


class TestGtkSettingsFile(SessionTestCase):
    def test_x11_gtk_programs_get_the_font_and_pointer(self):
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_app_theme
        finally:
            del sys.path[:2]
        theme = {"controls": {}, "fonts": {"interface": {"family": "Lucida Grande", "size": 13}},
                 "cursors": {"theme": "Aqua", "size": 32}}
        make_app_theme.write_gtk_settings(theme, self.home)
        written = (self.home / "xdg" / "gtk-3.0" / "settings.ini").read_text().splitlines()
        self.assertIn("gtk-font-name=Lucida Grande 9.75", written)
        self.assertIn("gtk-cursor-theme-name=Aqua", written)
        self.assertIn("gtk-cursor-theme-size=32", written)


class TestGameSettings(SessionTestCase):
    def setUp(self):
        super().setUp()
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        self.make = make_wayfire_config

    def test_variable_refresh_rate_per_screen(self):
        screens = {"DP-1": {"vrr": "always"}, "DP-2": {"vrr": "games"}, "HDMI-A-1": {}}
        config = {"display": {"screens": screens}}
        with unittest.mock.patch.object(self.make, "connected_screens", return_value={}):
            sections = {section[0]: section for section in self.make.display_sections(config)}
        self.assertIn("vrr = true", sections["[output:DP-1]"])
        self.assertNotIn("vrr = true", sections["[output:DP-2]"])
        self.assertNotIn("vrr = true", sections["[output:HDMI-A-1]"])
        self.assertEqual(self.make.game_vrr_section(config), (["[game-vrr]", "screens = DP-2"], ["game-vrr"]))
        self.assertEqual(self.make.game_vrr_section({}), (None, []))
        with self.assertRaises(SystemExit):
            self.make.game_vrr_section({"display": {"screens": {"DP-1": {"vrr": "sometimes"}}}})

    def test_screens_can_be_turned_off(self):
        config = {"display": {"screens": {"HDMI-A-1": {"enabled": False}}}}
        self.assertEqual(self.make.screens_off(config, ["eDP-1", "HDMI-A-1"]), {"HDMI-A-1"})
        # One screen always stays on.
        self.assertEqual(self.make.screens_off(config, ["HDMI-A-1"]), set())
        lid = {"power": {"lid_turns_off_screen": True}}
        with unittest.mock.patch.object(self.make, "lid_is_closed", return_value=True):
            self.assertEqual(self.make.screens_off(lid, ["eDP-1", "HDMI-A-1"]), {"eDP-1"})
            self.assertEqual(self.make.screens_off(lid, ["eDP-1"]), set())
            # Shut lid, external turned off by hand: the external one stays off.
            both = {**lid, **config}
            self.assertEqual(self.make.screens_off(both, ["eDP-1", "HDMI-A-1"]), {"HDMI-A-1"})
        with unittest.mock.patch.object(self.make, "lid_is_closed", return_value=False):
            self.assertEqual(self.make.screens_off(lid, ["eDP-1", "HDMI-A-1"]), set())
        screens = {"eDP-1": {"width": 1920, "height": 1080, "dpi": None, "sizes": {"1920x1080"}},
                   "HDMI-A-1": {"width": 1920, "height": 1080, "dpi": None, "sizes": {"1920x1080"}}}
        with unittest.mock.patch.object(self.make, "connected_screens", return_value=screens):
            sections = {section[0]: section for section in self.make.display_sections(config)}
        self.assertEqual(sections["[output:HDMI-A-1]"], ["[output:HDMI-A-1]", "mode = off"])
        self.assertNotIn("mode = off", sections["[output:eDP-1]"])
        # While the compositor shows only eDP-1 (HDMI-A-1 turned off), turning
        # HDMI-A-1 on again writes it on; a connector the compositor never
        # shows (a nested session's host screen) is no screen of ours.
        shown = {"eDP-1": {"width": 1920, "height": 1080}}
        with unittest.mock.patch.object(self.make, "connected_screens", return_value=screens), \
                unittest.mock.patch.object(self.make, "screen_modes", return_value=shown):
            on_again = {section[0]: section for section in self.make.display_sections({})}
            hidden_host = self.make.display_sections({"display": {"screens": {"eDP-1": {"enabled": False}}}})
        self.assertNotIn("mode = off", on_again["[output:HDMI-A-1]"])
        # eDP-1 is the only one shown, so it stays on.
        self.assertNotIn(["[output:eDP-1]", "mode = off"], hidden_host)

    def test_quit_can_close_the_whole_program(self):
        config = {"shortcuts": {"close_window": "<super> KEY_Q"}, "window": {"quit_closes_app": True}}
        self.assertEqual(self.make.with_quit_closing_app(config)["shortcuts"],
                         {"quit_app": "<super> KEY_Q", "close_window": ""})
        # Wayfire's own close key ("<super> KEY_Q | <alt> KEY_F4") must not come back.
        keys = self.make.compositor_key_sections(self.make.with_quit_closing_app(config))
        self.assertIn("close_top_view = none", keys["core"])
        own_key = {"shortcuts": {"close_window": "KEY_W", "quit_app": "KEY_Q"}, "window": {"quit_closes_app": True}}
        self.assertEqual(self.make.with_quit_closing_app(own_key)["shortcuts"], own_key["shortcuts"])
        off = {"shortcuts": {"close_window": "<super> KEY_Q"}}
        self.assertEqual(self.make.with_quit_closing_app(off)["shortcuts"], off["shortcuts"])
        command = self.make.shortcut_section(self.make.with_quit_closing_app(config), {})
        self.assertIn("binding_quit_app = <super> KEY_Q", command)

    def test_shake_sensitivity(self):
        self.assertEqual(self.make.shake_to_find_section({}), ["[shake-to-find]", "sensitivity = 3"])
        self.assertEqual(self.make.shake_to_find_section({"input": {"shake_sensitivity": 5}})[1], "sensitivity = 5")
        with self.assertRaises(SystemExit):
            self.make.shake_to_find_section({"input": {"shake_sensitivity": 9}})

    def test_x11_primary_and_middle_click_paste(self):
        self.assertEqual(self.make.x11_windows_section({}),
                         ["[x11-windows]", "games = true", "moves_need_button = true"])
        self.assertEqual(self.make.x11_windows_section({"window": {"borderless_full_screen": False}})[1],
                         "games = false")
        self.assertEqual(self.make.x11_windows_section({"window": {"x11_moves_need_button": False}})[2],
                         "moves_need_button = false")
        self.assertEqual(self.make.x11_settings_section({}),
                         ["[x11-settings]", "primary_screen = true", "dpi = false"])
        sharp = {"display": {"x11_primary": False, "sharp_x11": True}}
        self.assertEqual(self.make.x11_settings_section(sharp),
                         ["[x11-settings]", "primary_screen = false", "dpi = true"])
        self.assertIn("force_xwayland_scaling = true", self.make.workarounds_section(sharp))
        self.assertIn("force_xwayland_scaling = false", self.make.workarounds_section({}))
        self.assertIn("disable_primary_selection = false", self.make.workarounds_section({}))
        self.assertIn("disable_primary_selection = true",
                      self.make.workarounds_section({"input": {"middle_click_paste": False}}))
        with self.assertRaises(SystemExit):
            self.make.workarounds_section({"input": {"middle_click_paste": "no"}})

    def test_shake_to_find_loads_its_plugin_only_when_asked(self):
        self.assertEqual(self.make.shake_to_find_plugins({}), [])
        self.assertEqual(self.make.shake_to_find_plugins({"input": {"shake_to_find": True}}), ["shake-to-find"])
        with self.assertRaises(SystemExit):
            self.make.shake_to_find_plugins({"input": {"shake_to_find": "yes"}})


class TestGameKeys(SessionTestCase):
    def test_bare_function_keys_are_let_through_but_not_full_screens(self):
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
        finally:
            del sys.path[:2]
        section = make_wayfire_config.game_keys_section({
            "shortcuts": {
                "window_overview": "KEY_F3 | KEY_SCALE | <ctrl> KEY_UP",
                "show_desktop": "<super> KEY_F11",
                "app_grid": "KEY_F4",
                "fullscreen_window": "KEY_F11",
                "lock": "",
            },
            "commands": {"terminal": {"key": "KEY_F12", "run": ["foot"]}, "gone": None},
        })
        self.assertEqual(section, ["[game-keys]", "keys = KEY_F3 KEY_F4 KEY_F12",
                                   "programs = steam_app_* Minecraft*", "all_x11 = false"])
        own = make_wayfire_config.game_keys_section({"game_keys": {"games": ["osu!"], "all_x11": True}})
        self.assertEqual(own[2:], ["programs = osu!", "all_x11 = true"])
        with self.assertRaises(SystemExit):
            make_wayfire_config.game_keys_section({"game_keys": {"games": "steam_app_*"}})


class TestShellBlur(SessionTestCase):
    def setUp(self):
        super().setUp()
        sys.path[:0] = [self.folder.name, str(SESSION_SOURCE_DIR)]
        try:
            import make_wayfire_config
            self.generator = make_wayfire_config
        finally:
            del sys.path[:2]

    # The menu bar's translucency checkbox, and the Desktop pane's blur
    # checkboxes (blur.menu_bar and blur.dock_shelf).
    SWITCHED_ON = {"panel": {"translucent_menu_bar": True}}
    WANTED = {"menu_bar": True, "dock_shelf": True}

    def matcher(self, config, theme=None):
        section, plugins = self.generator.blur_section(config, theme or {})
        self.assertEqual(plugins, ["blur"] if section else [])
        return next((line for line in section if line.startswith("blur_by_default")), "")

    def test_blur_is_opt_in_and_targets_only_selected_shell_surfaces(self):
        self.assertEqual(self.generator.blur_section({}, {}), ([], []))
        for enabled, expected in (({"panel": True}, ['desktop-panel']),
                                  ({"dock": True}, ['mavernix-dock']),
                                  ({"panel": True, "dock": True}, ['desktop-panel', 'mavernix-dock'])):
            with self.subTest(enabled=enabled):
                section, plugins = self.generator.blur_section(
                    {"blur": {**enabled, **self.WANTED}, **self.SWITCHED_ON}, {})
                self.assertEqual(plugins, ["blur"])
                matcher = next(line for line in section if line.startswith("blur_by_default"))
                self.assertEqual(matcher, "blur_by_default = " + " | ".join(f'app_id is "{name}"' for name in expected))
                self.assertNotIn("toplevel", matcher)
                self.assertIn("toggle = none", section)
        self.assertEqual(self.generator.blur_section(
            {"blur": {"panel": False, **self.WANTED}, **self.SWITCHED_ON}, {"blur": {"panel": True}}), ([], []))

    def test_older_imports_blur_nothing_until_a_checkbox_is_on(self):
        # Sea Lion imports before 0.5.8 wrote panel = true and dock = true.
        older = {"blur": {"panel": True, "dock": True}}
        self.assertEqual(self.generator.blur_section(older, {}), ([], []))
        self.assertEqual(self.generator.blur_section({**older, **self.SWITCHED_ON}, {}), ([], []))

    def test_opaque_menu_bar_does_not_disable_dock_blur(self):
        matcher = self.matcher({"blur": {"panel": True, "dock": True, **self.WANTED},
                                "panel": {"translucent_menu_bar": False}})
        self.assertEqual(matcher, 'blur_by_default = app_id is "mavernix-dock"')

    def test_menu_bar_blur_is_independent_and_optional(self):
        for translucent, blurred, expected in ((True, False, False), (True, True, True),
                                                (False, True, False), (False, False, False)):
            with self.subTest(translucent=translucent, blurred=blurred):
                matcher = self.matcher({"panel": {"translucent_menu_bar": translucent},
                                        "blur": {"panel": True, "dock": True,
                                                 "menu_bar": blurred, "dock_shelf": True}})
                self.assertEqual('desktop-panel' in matcher, expected)
                self.assertIn('mavernix-dock', matcher)

    def test_dock_blur_is_its_own_checkbox(self):
        for wanted, expected in ((False, False), (True, True)):
            with self.subTest(wanted=wanted):
                matcher = self.matcher({"blur": {"dock": True, "dock_shelf": wanted}})
                self.assertEqual('mavernix-dock' in matcher, expected)

    def test_reflective_shelf_gets_dock_blur(self):
        config = {"dock": {"window_reflections": True}, "blur": {"dock": True, "dock_shelf": True}}
        lines, _ = self.generator.blur_section(config, {})
        self.assertIn('blur_by_default = app_id is "mavernix-dock-shelf"', lines)

    def test_strength_values_are_validated(self):
        for key, value in (("method", "bad"), ("iterations", -1), ("iterations", 2.5),
                           ("degrade", 0), ("saturation", 4), ("offset", float("nan"))):
            with self.subTest(key=key, value=value), self.assertRaises(SystemExit):
                self.generator.blur_section({"blur": {"panel": True, key: value, **self.WANTED}, **self.SWITCHED_ON}, {})


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

    def test_the_picked_icon_theme_is_locked_over_plasmas(self):
        # Unlocked, a Plasma user's ~/.config/kdeglobals wins and KDE programs
        # alone show Plasma's icons.
        icons = self.home / "data-dir" / "icons" / "Papirus"
        icons.mkdir(parents=True)
        (icons / "index.theme").write_text("[Icon Theme]\nName=Papirus\n")
        self.environment["XDG_DATA_DIRS"] = str(self.home / "data-dir")
        output = self.home / "app-theme"
        self.run_script("make_app_theme.py", THEMES_DIR / "plain", output, "--icon-theme", "Papirus")
        written = (output / "xdg" / "kdeglobals").read_text().splitlines()
        self.assertEqual(written[:2], ["[Icons][$i]", "Theme=Papirus"])
        self.assertIn("gtk-icon-theme-name=Papirus",
                      (output / "xdg" / "gtk-3.0" / "settings.ini").read_text().splitlines())


class TestLogGuard(unittest.TestCase):
    """The shell's log keeps repeated lines short, and a shell stuck saying
    one line over and over is ended so keep-running starts it again."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.log = Path(self.folder.name) / "login.log"
        self.pid_file = Path(self.folder.name) / "shell.pid"

    def guard(self, text):
        subprocess.run([sys.executable, SESSION_SOURCE_DIR / "log_guard.py", self.log, self.pid_file],
                       input=text.encode(), check=True, timeout=30)
        return self.log.read_text()

    def test_repeated_lines_are_counted(self):
        log = self.guard("starting\n" + "same\n" * 4 + "done\n")
        self.assertEqual(log, "starting\nsame\nlog_guard: the line above came 3 more times\ndone\n")

    def test_count_comes_at_the_end_too(self):
        log = self.guard("same\n" * 3)
        self.assertEqual(log, "same\nlog_guard: the line above came 2 more times\n")

    def test_stuck_shell_is_ended(self):
        # A stand-in shell: a program named qs, as the real one is.
        shell_program = Path(self.folder.name) / "qs"
        shutil.copy(shutil.which("sleep"), shell_program)
        shell = subprocess.Popen([shell_program, "60"])
        self.addCleanup(shell.kill)
        self.pid_file.write_text(f"{shell.pid}\n")
        log = self.guard("Invalid socket 285\n" * 6000)
        self.assertEqual(shell.wait(timeout=10), -15)
        self.assertIn("seems stuck; starting it again", log)
        self.assertLess(len(log), 1000)

    def test_stuck_shell_saying_two_things_is_ended(self):
        shell_program = Path(self.folder.name) / "qs"
        shutil.copy(shutil.which("sleep"), shell_program)
        shell = subprocess.Popen([shell_program, "60"])
        self.addCleanup(shell.kill)
        self.pid_file.write_text(f"{shell.pid}\n")
        log = self.guard("Invalid socket 285\nInvalid socket 286\n" * 3000)
        self.assertEqual(shell.wait(timeout=10), -15)
        self.assertIn("seems stuck; starting it again", log)

    def test_other_program_is_left_alone(self):
        other = subprocess.Popen(["sleep", "60"])
        self.addCleanup(other.kill)
        self.pid_file.write_text(f"{other.pid}\n")
        self.guard("Invalid socket 285\n" * 6000)
        self.assertIsNone(other.poll())


if __name__ == "__main__":
    unittest.main()
