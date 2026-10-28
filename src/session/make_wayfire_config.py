"""Writes the compositor's configuration from a desktop config and its theme.

    python make_wayfire_config.py data/config/plain.toml OUTPUT.ini

Joins the config (behaviour and layout) and the theme (looks) into Wayfire's
one ini file. `--extra FILE` appends sections unchanged, `--extra-plugins`
adds plugin names (both for dev/run-in-container), and `--autostart
NAME=COMMAND` (repeatable) starts a program with the session.
"""

import argparse
import json
import os
import shlex
import socket
import struct
import sys
import tomllib
from pathlib import Path

from paths import SESSION_DIR, SHELL_DIR
from themes import find_theme
from user_choices import load_desktop_config, with_user_choices

# Plugins every session loads; the order has no meaning to Wayfire.
BASE_PLUGINS = [
    "move", "resize", "place", "grid", "wm-actions", "window-rules",
    # Ours (src/compositor/).
    "themed-decoration",
    # Not "blur": its default rule blurs behind every window, a render pass
    # per frame for nothing.
    "animate", "scale", "expo", "vswitch",
    "foreign-toplevel", "gtk-shell", "kde-appmenu", "session-lock",
    "autostart", "command",
    "idle",
    "fast-switcher",
    # The control socket the shell talks to.
    "ipc", "ipc-rules",
    # Ours (src/compositor/).
    "window-pictures",
    "snap-keys",
    "shared-edges",
    "shortcut-capture",
    "screen-modes",
    "desktop-focus",
]


def fail(message):
    sys.exit(f"make_wayfire_config: {message}")


def load_toml(path):
    try:
        with open(path, "rb") as file:
            return tomllib.load(file)
    except FileNotFoundError:
        fail(f"{path} does not exist")
    except tomllib.TOMLDecodeError as error:
        fail(f"{path}: {error}")


def decoration_section(config, theme_dir):
    window = config.get("window", {})
    side = window.get("button_side", "left")
    if side not in ("left", "right"):
        fail(f'window.button_side is "{side}"; it must be "left" or "right"')
    order = window.get("button_order", ["close", "minimize", "maximize"])
    double_click = window.get("title_double_click", "maximize")
    if double_click not in ("maximize", "minimize", "none"):
        fail(f'window.title_double_click is "{double_click}"; '
             'it must be "maximize", "minimize" or "none"')

    # -1: the theme's own resize strip.
    resize_reach = int(window.get("resize_reach", -1))

    # Written as Wayfire window criteria ("app_id is ... | ..."), or "none".
    joined_apps = window.get("joined_tool_bar_apps", [])
    for app_id in joined_apps:
        if not isinstance(app_id, str) or '"' in app_id:
            fail(f"window.joined_tool_bar_apps holds {app_id!r}; it must list program ids as plain strings")
    joined_views = " | ".join(f'app_id is "{app_id}"' for app_id in joined_apps) or "none"

    return [
        "[themed-decoration]",
        f"theme = {theme_dir}",
        f"button_side = {side}",
        f"button_order = {' '.join(order)}",
        f"title_double_click = {double_click}",
        f"resize_grip = {resize_reach}",
        f"joined_views = {joined_views}",
    ]


# Theme curve names in Wayfire's terms; "ease-in" has no counterpart.
WAYFIRE_CURVES = {
    "linear": "linear",
    "ease-out": "circle",
    "ease-in-out": "sigmoid",
    "ease-in": "sigmoid",
}

# Minimise effects from our plugin; open/close effects from Wayfire's animate.
OUR_MINIMIZE_KINDS = ("genie", "scale")
WAYFIRE_WINDOW_KINDS = ("fade", "zoom")
# Theme name -> Wayfire grid plugin name, for maximise and tiling.
WAYFIRE_RESIZE_KINDS = {"blend": "crossfade", "none": "none"}


def animation_sections(theme):
    """Sections for window animations, and the plugins they need.

    Returns (sections, plugins). An animation the theme does not mention is
    switched off: a theme never gets an effect it did not ask for.
    """
    animations = theme.get("animation", {})
    animate = ["[animate]"]
    sections = [animate]
    plugins = []

    minimize = animations.get("minimize", {"kind": "none"})
    kind = minimize.get("kind", "none")
    if kind in OUR_MINIMIZE_KINDS:
        plugins.append("themed-minimize")
        # These effects ease themselves, so their clock must run linearly.
        sections.append([
            "[themed-minimize]",
            f"{kind}_duration = {minimize['duration_ms']}ms linear",
        ])
    elif kind != "none":
        fail(f'animation.minimize.kind is "{kind}"; it must be one of '
             f'{", ".join(OUR_MINIMIZE_KINDS)} or "none"')
    animate.append(f"minimize_animation = {kind}")

    # Wayfire has one duration per kind of effect, shared by opening and closing.
    durations = {}
    for name, option in (("window_open", "open_animation"), ("window_close", "close_animation")):
        animation = animations.get(name, {"kind": "none"})
        kind = animation.get("kind", "none")
        if kind not in WAYFIRE_WINDOW_KINDS + ("none",):
            fail(f'animation.{name}.kind is "{kind}"; it must be one of '
                 f'{", ".join(WAYFIRE_WINDOW_KINDS)} or "none"')
        animate.append(f"{option} = {kind}")
        if kind == "none":
            continue

        curve = animation.get("curve", "linear")
        if curve not in WAYFIRE_CURVES:
            fail(f'animation.{name}.curve is "{curve}"; it must be one of {", ".join(WAYFIRE_CURVES)}')
        duration = f"{animation['duration_ms']}ms {WAYFIRE_CURVES[curve]}"
        if durations.get(kind, duration) != duration:
            fail(f"animation.window_open and animation.window_close both use \"{kind}\" but with "
                 "different timing; Wayfire has one timing per kind of effect")
        durations[kind] = duration

    for kind, duration in durations.items():
        animate.append(f"{kind}_duration = {duration}")

    sections.append(resize_animation_section(animations))
    return sections, plugins


def shorten_open_and_close(animate_section, duration_ms):
    """Overrides the open/close duration ([window] quick_open_close_ms),
    keeping the theme's curve."""
    for index, line in enumerate(animate_section):
        name, _, value = line.partition(" = ")
        if name in ("fade_duration", "zoom_duration"):
            curve = value.partition(" ")[2]
            animate_section[index] = f"{name} = {int(duration_ms)}ms {curve}"


def resize_animation_section(animations):
    """The grid plugin's animation section.

    Written even for "none", or the plugin animates with its own defaults.
    """
    animation = animations.get("window_resize", {"kind": "none"})
    kind = animation.get("kind", "none")
    if kind not in WAYFIRE_RESIZE_KINDS:
        fail(f'animation.window_resize.kind is "{kind}"; it must be one of '
             f'{", ".join(WAYFIRE_RESIZE_KINDS)}')
    section = ["[grid]", f"type = {WAYFIRE_RESIZE_KINDS[kind]}"]
    if kind != "none":
        curve = animation.get("curve", "linear")
        if curve not in WAYFIRE_CURVES:
            fail(f'animation.window_resize.curve is "{curve}"; it must be one of '
                 f'{", ".join(WAYFIRE_CURVES)}')
        section.append(f"duration = {animation['duration_ms']}ms {WAYFIRE_CURVES[curve]}")
    return section


# Actions of the shell's run() (shell.qml).
SHELL_ACTIONS = ["app_grid", "volume_up", "volume_down", "volume_mute", "lock", "notification_list", "search",
                 "switch_program", "switch_program_back"]
# Repeated while the key is held.
REPEATING_ACTIONS = ["volume_up", "volume_down"]

# [shortcuts] action -> (plugin, option) that takes the key.
COMPOSITOR_ACTIONS = {
    "window_overview": ("scale", "toggle"),
    "close_window": ("core", "close_top_view"),
    "switch_window": ("fast-switcher", "activate"),
    "minimize_window": ("wm-actions", "minimize"),
    "fullscreen_window": ("wm-actions", "toggle_fullscreen"),
    "show_desktop": ("wm-actions", "toggle_showdesktop"),
    "previous_desktop": ("vswitch", "binding_left"),
    "next_desktop": ("vswitch", "binding_right"),
    "window_to_previous_desktop": ("vswitch", "with_win_left"),
    "window_to_next_desktop": ("vswitch", "with_win_right"),
    # Each press goes on from where the window is.
    "snap_left": ("snap-keys", "left"),
    "snap_right": ("snap-keys", "right"),
    "snap_up": ("snap-keys", "up"),
    "snap_down": ("snap-keys", "down"),
    "tile_left": ("grid", "slot_l"),
    "tile_right": ("grid", "slot_r"),
    "tile_top": ("grid", "slot_t"),
    "tile_bottom": ("grid", "slot_b"),
    "tile_top_left": ("grid", "slot_tl"),
    "tile_top_right": ("grid", "slot_tr"),
    "tile_bottom_left": ("grid", "slot_bl"),
    "tile_bottom_right": ("grid", "slot_br"),
    "maximize_window": ("grid", "slot_c"),
    "restore_window": ("grid", "restore"),
}

# Bindings the grid plugin has by default; unbound unless the config sets them.
GRID_OWN_BINDINGS = [
    "slot_l", "slot_r", "slot_t", "slot_b", "slot_tl", "slot_tr", "slot_bl", "slot_br",
    "slot_c", "restore",
]

# [desktops] still / slides_as_two names -> the shell's surface names.
STILL_PARTS = {"dock": "mavernix-dock", "panel": "desktop-panel"}

# vswitch's default bindings; unbound unless the config sets them.
VSWITCH_OWN_BINDINGS = [
    "binding_left", "binding_right", "binding_up", "binding_down",
    "with_win_left", "with_win_right", "with_win_up", "with_win_down",
]

# Compositor actions reachable over the IPC socket (for gestures).
COMPOSITOR_METHODS = {
    "window_overview": "scale/toggle",
    "show_desktop": "wm-actions/toggle_showdesktop",
}

# [gestures] keys: the touchpad-gestures plugin's option names.
GESTURES = [f"{kind}_{fingers}"
            for kind in ("swipe_up", "swipe_down", "swipe_left", "swipe_right", "pinch_in", "pinch_out")
            for fingers in (3, 4)]

# Written by `localectl set-x11-keymap`.
SYSTEM_KEYBOARD_FILE = Path("/etc/X11/xorg.conf.d/00-keyboard.conf")


def system_keyboard(path=SYSTEM_KEYBOARD_FILE):
    """The machine's keyboard settings: { "layout": "de", "variant": ..., ... }.

    Parses `Option "XkbLayout" "de"` lines; empty if there is no file.
    """
    names = {"XkbLayout": "layout", "XkbVariant": "variant",
             "XkbModel": "model", "XkbOptions": "options"}
    found = {}
    try:
        text = path.read_text()
    except OSError:
        return found
    for line in text.splitlines():
        words = shlex.split(line, comments=True)
        if len(words) == 3 and words[0] == "Option" and words[1] in names:
            found[names[words[1]]] = words[2]
    return found


# One folder per connector, named like "card1-eDP-1".
SCREENS_DIRECTORY = Path("/sys/class/drm")

# Screens from this many pixels per inch get scale 2 under "auto". Ordinary
# laptop panels are around 130, high-density ones 190 and up.
DENSE_SCREEN_DPI = 170


def connected_screens(directory=SCREENS_DIRECTORY):
    """The screens that are plugged in:
    { "eDP-1": { "width": 2560, "dpi": 191.0, "sizes": {"2560x1600"} } }.

    The density comes from the best mode and the EDID's physical width
    (byte 21, centimetres); None if the screen reports no size.
    """
    screens = {}
    for folder in sorted(directory.glob("card*-*")):
        try:
            if (folder / "status").read_text().strip() != "connected":
                continue
            sizes = (folder / "modes").read_text().split()
            width = int(sizes[0].split("x")[0])
            edid = (folder / "edid").read_bytes()
        except (OSError, ValueError):
            continue
        name = folder.name.split("-", 1)[1]
        width_cm = edid[21] if len(edid) > 22 else 0
        dpi = width / (width_cm / 2.54) if width_cm > 0 else None
        screens[name] = {"width": width, "dpi": dpi, "sizes": set(sizes)}
    return screens


# CVT reduced blanking (VESA CVT 1.2, as `cvt -r` makes them): a custom mode's
# timings. A laptop's panel keeps its own; the graphics card stretches the
# picture to it.
CVT_RB_MIN_VBLANK_US = 460
CVT_RB_H_BLANK = 160
CVT_RB_H_SYNC = 32
CVT_RB_V_FRONT = 3
CVT_RB_MIN_V_BACK = 6
CVT_CLOCK_STEP_MHZ = 0.25


def cvt_reduced_modeline(width, height, refresh_hz):
    """A modeline as Wayfire's custom_mode reads it: "clock hdisplay ... -vsync"."""
    width -= width % 8
    # The sync's length tells the shape apart.
    v_sync = next((lines for shape, lines in ((4 / 3, 4), (16 / 9, 5), (16 / 10, 6), (5 / 4, 7), (15 / 9, 7))
                   if abs(width / height - shape) < 0.01), 10)
    line_us = (1_000_000 / refresh_hz - CVT_RB_MIN_VBLANK_US) / height
    blank_lines = max(int(CVT_RB_MIN_VBLANK_US / line_us) + 1, CVT_RB_V_FRONT + v_sync + CVT_RB_MIN_V_BACK)
    h_total = width + CVT_RB_H_BLANK
    v_total = height + blank_lines
    clock_mhz = CVT_CLOCK_STEP_MHZ * int(refresh_hz * h_total * v_total / 1_000_000 / CVT_CLOCK_STEP_MHZ)
    h_sync_end = h_total - CVT_RB_H_BLANK // 2
    v_sync_start = height + CVT_RB_V_FRONT
    return (f"{clock_mhz:.2f} {width} {h_sync_end - CVT_RB_H_SYNC} {h_sync_end} {h_total} "
            f"{height} {v_sync_start} {v_sync_start + v_sync} {v_total} +hsync -vsync")


def custom_mode(mode, screen):
    """The modeline for `mode` ("1920x1200@120000") if the screen does not list
    its size, else None: Wayfire sets only listed sizes on a real screen."""
    try:
        size, refresh = str(mode).split("@")
        width, height = map(int, size.split("x"))
        refresh_hz = int(refresh) / 1000
    except ValueError:
        return None
    if "sizes" not in screen or size in screen["sizes"] or refresh_hz <= 0:
        return None
    return cvt_reduced_modeline(width, height, refresh_hz)


def screen_positions():
    """Where the running compositor has put each screen: { "HDMI-A-1": "1920,0" }.

    Empty outside a session, or if the compositor does not answer.
    """
    path = os.environ.get("WAYFIRE_SOCKET")
    if not path:
        return {}
    body = json.dumps({"method": "window-rules/list-outputs", "data": {}}).encode()
    try:
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(2)
            connection.connect(path)
            connection.sendall(struct.pack("<I", len(body)) + body)
            (length,) = struct.unpack("<I", connection.recv(4, socket.MSG_WAITALL))
            outputs = json.loads(connection.recv(length, socket.MSG_WAITALL))
    except (OSError, ValueError, struct.error):
        return {}
    if not isinstance(outputs, list):
        return {}
    return {output["name"]: f'{int(output["geometry"]["x"])},{int(output["geometry"]["y"])}'
            for output in outputs if "name" in output and "geometry" in output}


def display_sections(config):
    """Output sections from the config's [display] table.

    `scale` is a number or "auto"; [display.screens.NAME] overrides one
    screen. A screen plugged in later gets the compositor's default, 1.
    With several screens, one the config does not place keeps where it is
    now: rewriting the config would otherwise undo an arrangement made
    while the session runs.
    """
    wishes = config.get("display", {})
    general = wishes.get("scale", "auto")
    single = wishes.get("screens", {})
    sections = []
    names = sorted(set(connected_screens()) | set(single))
    screens = connected_screens()
    current = screen_positions()
    if len(current) < 2:
        current = {}
    for name in names:
        own = single.get(name, {})
        scale = own.get("scale", general)
        if scale == "auto":
            dpi = screens.get(name, {}).get("dpi")
            # A smaller mode stretched to the panel is that much less dense.
            native_width = screens.get(name, {}).get("width")
            try:
                chosen_width = int(str(own["mode"]).split("x")[0])
            except (KeyError, ValueError):
                chosen_width = None
            if dpi is not None and native_width and chosen_width:
                dpi = dpi * chosen_width / native_width
            scale = 2 if dpi is not None and dpi >= DENSE_SCREEN_DPI else 1
        lines = [f"[output:{name}]", f"scale = {scale}"]
        for key in ("mode", "position", "transform"):
            if key in own:
                lines.append(f"{key} = {own[key]}")
        if "position" not in own and name in current:
            lines.append(f"position = {current[name]}")
        modeline = custom_mode(own["mode"], screens.get(name, {})) if "mode" in own else None
        if modeline is not None:
            lines.append(f"custom_mode = {modeline}")
        sections.append(lines)
    return sections


def yes_or_no(value):
    return "true" if value else "false"


# The pointers' size when the theme names none, as Wayfire's own default.
DEFAULT_CURSOR_SIZE = 24

# [input] keyboard_switch_key -> XKB option.
LAYOUT_SWITCH_OPTIONS = {
    "ctrl_space": "grp:ctrl_space_toggle",
    "alt_shift": "grp:alt_shift_toggle",
    "caps_lock": "grp:caps_toggle",
}


def keyboard_lines(config):
    """The compositor's `xkb_*` lines, from the config's [input] table.

    The config's layout wins over the machine's. A config that names layouts
    names their variants too, so the machine's variant is not carried over.
    """
    wishes = config.get("input", {})
    machine = system_keyboard()
    if "keyboard_layout" in wishes:
        machine.pop("layout", None)
        machine.pop("variant", None)

    values = {}
    for name in ("layout", "variant", "model", "options"):
        value = wishes.get(f"keyboard_{name}", machine.get(name))
        if value:
            values[name] = value

    switch_key = wishes.get("keyboard_switch_key", "")
    if switch_key:
        if switch_key not in LAYOUT_SWITCH_OPTIONS:
            fail(f'[input] keyboard_switch_key is "{switch_key}"; '
                 f"it must be one of {', '.join(LAYOUT_SWITCH_OPTIONS)}, or empty for none")
        # With one layout the option would only steal the key from programs.
        if "," in values.get("layout", ""):
            options = [values["options"]] if "options" in values else []
            options.append(LAYOUT_SWITCH_OPTIONS[switch_key])
            values["options"] = ",".join(options)

    return [f"xkb_{name} = {value}" for name, value in values.items()]


def input_section(config, theme):
    """Keyboard and touchpad settings, from the config's [input] table, and
    the theme's mouse pointers ([cursors]; else the system's)."""
    wishes = config.get("input", {})
    lines = ["[input]"]
    lines += keyboard_lines(config)
    cursors = theme.get("cursors", {})
    lines.append(f"cursor_theme = {cursors.get('theme', 'default')}")
    lines.append(f"cursor_size = {cursors.get('size', DEFAULT_CURSOR_SIZE)}")
    if "key_repeat_delay_ms" in wishes:
        lines.append(f"kb_repeat_delay = {wishes['key_repeat_delay_ms']}")
    if "key_repeat_per_second" in wishes:
        lines.append(f"kb_repeat_rate = {wishes['key_repeat_per_second']}")
    if "tap_to_click" in wishes:
        lines.append(f"tap_to_click = {yes_or_no(wishes['tap_to_click'])}")
    if "secondary_click" in wishes:
        methods = {"two_fingers": "clickfinger", "corner": "button-areas"}
        if wishes["secondary_click"] not in methods:
            fail(f'[input] secondary_click is "{wishes["secondary_click"]}"; '
                 f"it must be one of {', '.join(methods)}")
        lines.append(f"click_method = {methods[wishes['secondary_click']]}")
    if "natural_scroll" in wishes:
        lines.append(f"natural_scroll = {yes_or_no(wishes['natural_scroll'])}")
    if "mouse_natural_scroll" in wishes:
        lines.append(f"mouse_natural_scroll = {yes_or_no(wishes['mouse_natural_scroll'])}")
    if "mouse_scroll_speed" in wishes:
        scroll = wishes["mouse_scroll_speed"]
        if isinstance(scroll, bool) or not isinstance(scroll, (int, float)) or scroll <= 0:
            fail(f"[input] mouse_scroll_speed is {scroll!r}; it must be a number above 0 (1 is unchanged)")
        lines.append(f"mouse_scroll_speed = {scroll}")
    if "left_handed" in wishes:
        lines.append(f"left_handed_mode = {yes_or_no(wishes['left_handed'])}")
    for device in ("mouse", "touchpad"):
        if f"{device}_speed" in wishes:
            speed = wishes[f"{device}_speed"]
            if isinstance(speed, bool) or not isinstance(speed, (int, float)) or not -1 <= speed <= 1:
                fail(f"[input] {device}_speed is {speed!r}; it must be a number from -1 to 1")
            lines.append(f"{device}_cursor_speed = {speed}")
        if f"{device}_acceleration" in wishes:
            profile = "adaptive" if wishes[f"{device}_acceleration"] else "flat"
            lines.append(f"{device}_accel_profile = {profile}")
    return lines


def idle_section(config):
    """Screen blanking from [idle]; locking is the shell's."""
    seconds = config.get("idle", {}).get("screen_off_after_seconds", 0)
    return [
        "[idle]",
        # -1 is "never".
        f"dpms_timeout = {seconds if seconds > 0 else -1}",
        "screensaver_timeout = -1",
    ]


def command_lines(config):
    """Keys that run a program, from the config's [commands] table.

    Compositor bindings, so they work even while the shell is down.
    """
    lines = []
    for name, entry in config.get("commands", {}).items():
        # null is how a user override deletes a command.
        if entry is None or not entry.get("key"):
            continue
        if "run" not in entry:
            fail(f'[commands] {name} needs a `key` and a `run` list')
        binding = "repeatable_binding" if entry.get("repeat", False) else "binding"
        lines += [
            f"{binding}_run_{name} = {entry['key']}",
            f"command_run_{name} = {shlex.join(entry['run'])}",
        ]
    return lines


def compositor_key_sections(config):
    """Keys for COMPOSITOR_ACTIONS other than the window overview.

    An empty key becomes "none"; left out, a plugin keeps its default key.
    """
    shortcuts = config.get("shortcuts", {})
    by_section = {}
    for action, (section, option) in COMPOSITOR_ACTIONS.items():
        if action == "window_overview" or action not in shortcuts:
            continue
        key = shortcuts[action] or "none"
        by_section.setdefault(section, []).append(f"{option} = {key}")
    return by_section


def shell_actions(theme):
    """Shell actions under this theme, including the window overview when
    the theme has the shell draw it."""
    drawn_by = theme.get("window_overview", {}).get("drawn_by", "compositor")
    if drawn_by not in ("compositor", "shell"):
        fail(f'window_overview.drawn_by is "{drawn_by}"; it must be "compositor" or "shell"')
    return SHELL_ACTIONS + (["window_overview"] if drawn_by == "shell" else [])


def shell_command(action):
    """The command that asks the running shell to do something."""
    return f"qs -p {SHELL_DIR} ipc call shell run {action}"


def gesture_section(config, theme):
    """The [gestures] table; returns (section, plugins).

    Without the table the plugin is not loaded.
    """
    gestures = config.get("gestures", {})
    if not gestures:
        return [], []

    lines = ["[touchpad-gestures]"]
    for gesture, wish in gestures.items():
        if gesture in ("swipe_distance", "pinch_change"):
            lines.append(f"{gesture} = {wish}")
            continue
        if gesture not in GESTURES:
            fail(f'[gestures] names "{gesture}", which is no gesture; known: {", ".join(GESTURES)}')

        if isinstance(wish, dict):
            if "run" not in wish:
                fail(f"[gestures] {gesture} is a table and so needs a `run` list")
            command = shlex.join(wish["run"])
        elif wish in shell_actions(theme):
            command = shell_command(wish)
        elif wish in COMPOSITOR_METHODS:
            command = f"python3 {SESSION_DIR / 'compositor_call.py'} {COMPOSITOR_METHODS[wish]}"
        else:
            known = shell_actions(theme) + list(COMPOSITOR_METHODS)
            fail(f'[gestures] {gesture} asks for "{wish}", which a gesture cannot do; '
                 f"known: {', '.join(known)}, or a table with `run`")
        lines.append(f"{gesture} = {command}")
    return lines, ["touchpad-gestures"]


def shortcut_section(config, theme):
    """The command plugin's bindings: shell [shortcuts] and [commands]."""
    shortcuts = config.get("shortcuts", {})
    lines = command_lines(config)
    for action, key in shortcuts.items():
        if action in shell_actions(theme):
            pass
        elif action in COMPOSITOR_ACTIONS:
            continue
        else:
            known = SHELL_ACTIONS + list(COMPOSITOR_ACTIONS)
            fail(f'[shortcuts] names "{action}", which the desktop cannot do; '
                 f"known: {', '.join(known)}")
        if not key:
            continue
        binding = "repeatable_binding" if action in REPEATING_ACTIONS else "binding"
        lines += [
            f"{binding}_{action} = {key}",
            f"command_{action} = {shell_command(action)}",
        ]
    return ["[command]"] + lines if lines else []


def window_handling_sections(config, theme, grid_section, grid_key_lines):
    """Moving, resizing and snapping windows, from the config's [window].

    Returns the move, resize and shared-edges sections; adds keys and an
    optional quicker animation to `grid_section`.
    """
    window = config.get("window", {})
    preview = theme.get("window", {}).get("snap_preview", {})

    given = {line.split(" = ")[0] for line in grid_key_lines}
    grid_section += grid_key_lines
    grid_section += [f"{binding} = none" for binding in GRID_OWN_BINDINGS if binding not in given]
    quick_ms = window.get("quick_resize_ms")
    if quick_ms is not None:
        grid_section[:] = [line for line in grid_section if not line.startswith("duration = ")]
        grid_section.append(f"duration = {int(quick_ms)}ms circle")

    # Held while dragging anywhere on a window: left moves, right resizes.
    drag_key = window.get("drag_key", "")
    move_with = f"{drag_key} BTN_LEFT" if drag_key else "none"
    resize_with = f"{drag_key} BTN_RIGHT" if drag_key else "none"

    snaps = window.get("snap_to_edges", False)
    # -1 is Wayfire's "never".
    to_next_desktop_ms = int(config.get("desktops", {}).get("drag_to_edge_ms", -1))
    move = [
        "[move]",
        f"activate = {move_with}",
        f"enable_snap = {yes_or_no(snaps)}",
        f"enable_snap_off = {yes_or_no(snaps)}",
        f"snap_threshold = {int(window.get('snap_distance', 10))}",
        f"quarter_snap_threshold = {int(window.get('snap_corner_distance', 50))}",
        f"workspace_switch_after = {to_next_desktop_ms}",
        f"preview_base_color = {wayfire_color(preview.get('color', '#80808040'))}",
        f"preview_base_border = {wayfire_color(preview.get('border_color', '#808080c0'))}",
        f"preview_border_width = {int(preview.get('border_width', 2))}",
    ]
    shared_edges = ["[shared-edges]", f"enabled = {yes_or_no(window.get('shared_edges', True))}"]
    return [move, ["[resize]", f"activate = {resize_with}"], shared_edges]


def wayfire_curve(curve, where):
    """A theme's curve as Wayfire writes one.

    A curve is a name from WAYFIRE_CURVES or four CSS cubic-bezier numbers;
    `where` names the theme entry for error messages.
    """
    if isinstance(curve, list):
        if len(curve) != 4 or not all(isinstance(number, (int, float)) for number in curve):
            fail(f"{where} is a list and so must be four numbers, [x1, y1, x2, y2]")
        return "cubic-bezier " + " ".join(str(number) for number in curve)
    if curve not in WAYFIRE_CURVES:
        fail(f'{where} is "{curve}"; it must be one of {", ".join(WAYFIRE_CURVES)} or four numbers')
    return WAYFIRE_CURVES[curve]


def most_desktops(config):
    """How many desktops there can be at most ([desktops] most)."""
    return int(config.get("desktops", {}).get("most", 16))


def desktop_count(config):
    """How many desktops the session starts with ([desktops] count)."""
    count = int(config.get("desktops", {}).get("count", 1))
    if not 1 <= count <= most_desktops(config):
        fail(f"[desktops] count is {count}; it must be between 1 and {most_desktops(config)}")
    return count


def desktop_sections(config, theme, key_lines):
    """Desktops as one row of Wayfire workspaces, switched by vswitch
    (keys) and vswipe (touchpad). Returns (lines for [core], sections, plugins).
    """
    wishes = config.get("desktops", {})
    looks = theme.get("desktops", {})
    motion = theme.get("animation", {}).get("desktop_switch", {})

    core = [f"vwidth = {desktop_count(config)}", "vheight = 1"]

    gap = looks.get("gap", 0)
    gap_color = wayfire_color(looks.get("gap_color", "#000000"))
    duration = "0ms linear"
    if motion:
        curve = wayfire_curve(motion.get("curve", "linear"), "animation.desktop_switch.curve")
        duration = f"{motion['duration_ms']}ms {curve}"

    given = {line.split(" = ")[0] for line in key_lines}
    vswitch = ["[vswitch]"] + key_lines
    vswitch += [f"{binding} = none" for binding in VSWITCH_OWN_BINDINGS if binding not in given]
    vswitch += [
        f"gap = {gap}",
        f"background = {gap_color}",
        f"duration = {duration}",
        "wraparound = false",
    ]
    sections = [vswitch]
    plugins = []

    # still-surfaces keeps shell parts still while desktops slide, or slides
    # them "as two": the old desktop keeps a snapshot, the new one the live part.
    still = wishes.get("still", [])
    as_two = wishes.get("slides_as_two", [])
    for key, parts in (("still", still), ("slides_as_two", as_two)):
        for part in parts:
            if part not in STILL_PARTS:
                fail(f'[desktops] {key} names "{part}"; known: {", ".join(STILL_PARTS)}')
    if still or as_two:
        sections.append([
            "[still-surfaces]",
            "names = " + " ".join(STILL_PARTS[part] for part in still),
            "sliding = " + " ".join(STILL_PARTS[part] for part in as_two),
        ])
        plugins.append("still-surfaces")

    fingers = int(wishes.get("swipe_fingers", 0))
    if fingers:
        sections.append([
            "[vswipe]",
            f"fingers = {fingers}",
            "enable_horizontal = true",
            # Vertical swipes belong to [gestures].
            "enable_vertical = false",
            f"gap = {gap}",
            f"background = {gap_color}",
        ])
        plugins.append("vswipe")
    return core, sections, plugins


def wayfire_color(text):
    """ "#rrggbb" or "#rrggbbaa" as Wayfire's four 0..1 numbers."""
    digits = text.lstrip("#")
    if len(digits) == 6:
        digits += "ff"
    return " ".join(f"{int(digits[index:index + 2], 16) / 255:.3f}" for index in (0, 2, 4, 6))


def window_overview_section(config, theme):
    """The scale plugin (window overview): its key from the config, its
    looks from the theme's [window_overview]."""
    lines = ["[scale]"]
    key = config.get("shortcuts", {}).get("window_overview") or None
    if "window_overview" in shell_actions(theme):
        key = None
    # Explicit "none", or the plugin keeps its own default key.
    lines.append(f"toggle = {key if key is not None else 'none'}")

    looks = theme.get("window_overview", {})
    if "spacing" in looks:
        lines.append(f"spacing = {looks['spacing']}")
    if "outer_margin" in looks:
        lines.append(f"outer_margin = {looks['outer_margin']}")
    if "other_windows_opacity" in looks:
        lines.append(f"inactive_alpha = {looks['other_windows_opacity']}")
    if "titles" in looks:
        names = {"all": "all", "pointer": "mouse", "none": "never"}
        if looks["titles"] not in names:
            fail(f'window_overview.titles is "{looks["titles"]}"; it must be one of {", ".join(names)}')
        lines.append(f"title_overlay = {names[looks['titles']]}")
    if "title_font_size" in looks:
        lines.append(f"title_font_size = {looks['title_font_size']}")
    if "title_position" in looks:
        lines.append(f"title_position = {looks['title_position']}")
    if "title_color" in looks:
        lines.append(f"text_color = {wayfire_color(looks['title_color'])}")
    if "title_background" in looks:
        lines.append(f"bg_color = {wayfire_color(looks['title_background'])}")
    duration = theme.get("animation", {}).get("window_overview", {}).get("duration_ms")
    if duration is not None:
        lines.append(f"duration = {duration}ms")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("config", type=Path, help="desktop config, e.g. data/config/plain.toml")
    parser.add_argument("output", type=Path, help="Wayfire config file to write")
    parser.add_argument("--extra", type=Path, help="file with further sections to append")
    parser.add_argument("--extra-plugins", default="", help="further plugin names, space-separated")
    parser.add_argument(
        "--autostart", action="append", default=[], metavar="NAME=COMMAND",
        help="a program to start with the session; may be given several times",
    )
    arguments = parser.parse_args()

    config = with_user_choices(load_desktop_config(arguments.config), arguments.config)
    if "theme" not in config:
        fail(f"{arguments.config} does not name a theme")
    theme_dir = find_theme(config["theme"])
    theme = load_toml(theme_dir / "theme.toml")

    plugins = list(BASE_PLUGINS)
    sections = [decoration_section(config, theme_dir)]

    animations, animation_plugins = animation_sections(theme)
    quick_open_close_ms = config.get("window", {}).get("quick_open_close_ms")
    if quick_open_close_ms is not None:
        shorten_open_and_close(animations[0], quick_open_close_ms)
    sections += animations
    plugins += animation_plugins

    shortcuts = shortcut_section(config, theme)
    if shortcuts:
        sections.append(shortcuts)
    sections.append(window_overview_section(config, theme))
    sections.append(input_section(config, theme))
    sections.append(idle_section(config))
    sections += display_sections(config)
    gestures, gesture_plugins = gesture_section(config, theme)
    if gestures:
        sections.append(gestures)
    plugins += gesture_plugins
    compositor_keys = compositor_key_sections(config)
    desktop_core, desktop_parts, desktop_plugins = desktop_sections(
        config, theme, compositor_keys.pop("vswitch", []))
    compositor_keys.setdefault("core", []).extend(desktop_core)
    sections += desktop_parts
    plugins += desktop_plugins
    grid_section = next(section for section in sections if section[0] == "[grid]")
    sections += window_handling_sections(config, theme, grid_section, compositor_keys.pop("grid", []))
    for section, lines in compositor_keys.items():
        if section != "core":
            sections.append([f"[{section}]"] + lines)

    plugins += arguments.extra_plugins.split()

    lines = [
        f"# Written by make_wayfire_config.py from {arguments.config.name}",
        f"# and the theme in {theme_dir}. Do not edit; change those instead.",
        "",
        "[core]",
        f"plugins = {' '.join(plugins)}",
        *compositor_keys.get("core", []),
        "",
        "# Our shell replaces Wayfire's own panel and dock.",
        "[autostart]",
        "autostart_wf_shell = false",
    ]
    for entry in arguments.autostart:
        name, separator, command = entry.partition("=")
        if not separator or not command:
            fail(f'--autostart "{entry}" must have the form NAME=COMMAND')
        lines.append(f"{name} = {command}")
    for section in sections:
        lines += [""] + section

    text = "\n".join(lines) + "\n"
    if arguments.extra is not None:
        text += "\n" + arguments.extra.read_text()

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    # Wayfire reloads the file as soon as it changes; written in place, it
    # could read half of it, and a half-read config has crashed it. A
    # reconfigure started while another runs writes its own copy too.
    unfinished = arguments.output.with_name(f"{arguments.output.name}.{os.getpid()}.new")
    unfinished.write_text(text)
    os.replace(unfinished, arguments.output)
    print(f"wrote {arguments.output} (theme: {theme_dir.name}, plugins: {len(plugins)})")


if __name__ == "__main__":
    main()
