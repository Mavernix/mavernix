# How the desktop is put together

Mavernix is a session made of existing parts: [Wayfire](https://github.com/WayfireWM/wayfire) is the compositor, [Quickshell](https://quickshell.org) runs the shell, Qt draws the desktop's own programs. Our code supplies what none of them has: the panel, dock and overlays, a few compositor plugins, the session that ties it together, and a theme format that every part reads.

## Three layers

- **Code** implements generic desktop parts: panel, dock, window frames, window overview, app grid, notifications, search. It knows nothing about any particular look.
- **A theme** (`data/themes/NAME/theme.toml` and its pictures) holds everything about looks: sizes, colours, fonts, pictures, sounds, and every animation's kind, duration and curve. See [the theme format](themes/README.md).
- **A desktop config** (`data/config/NAME.toml`, or one a look brings) holds behaviour and layout: which parts are shown and where, the window buttons' side, keys, gestures, the dock's programs. See [configuration](configuration.md).

The rule that keeps them apart: a different theme and config must be able to make a different desktop without touching code. If something cannot be expressed, the theme format or config gains a key; the code does not learn about a theme.

## The parts

| Folder | What it is | Language |
|---|---|---|
| `src/session/` | starts the desktop: turns a config and its theme into the compositor's and the shell's settings, sets up the environment, then runs Wayfire | shell scripts, Python |
| `src/compositor/` | Wayfire plugins: window frames drawn from the theme, the minimise effects, pictures of single windows for the dock, touchpad gestures, keys that tile windows, shared edges between tiled windows, parts of the shell that stay put while desktops slide, recording a shortcut | C++ |
| `src/shell/` | the panel, dock, desktop icons, menus, notifications, app grid, window overview, search, lock screen | QML, run by Quickshell |
| `src/shell/native/` | a QML module for what QML cannot do: application menus over D-Bus, file kinds, icons, folder watching, the compositor's socket | C++ |
| `src/apps/settings/` | the settings window | C++, Qt Widgets |
| `src/apps/theme-installer/` | the Theme Installer: a window over a back end (`backend/`) that makes a look's themes from an installer the user owns; each look is a folder in `backend/looks/` ([looks](looks.md)) | C++ with QML; Python |
| `data/` | what is installed and read: configs, themes, looks for other programs, the sound pack | TOML, pictures |
| `tests/` | the tests | C++, Python |
| `dev/` | trying the desktop in a container, and small tools that made some of the data | shell, Python |

## How they talk

1. **At login**, `start-desktop` reads the chosen desktop config, finds its theme, and has `configure-desktop` write the session's state into `$XDG_RUNTIME_DIR/mavernix/`:
   - `session.ini`, Wayfire's settings (`make_wayfire_config.py`), which load our plugins and pass them the theme's folder;
   - `shell.json`, the config and the theme for the shell and the programs (`make_shell_config.py`);
   - toolkit themes for GTK and Qt programs, made from the theme's `[controls]` (`make_app_theme.py`);
   - desktop entries for the desktop's own programs, and the environment for everything started in the session.

   Then it becomes Wayfire, which starts the shell.
2. **The shell and the programs** read `shell.json` (its path is in `DESKTOP_SHELL_SETTINGS`). What a user changes while the desktop runs (dock contents, the settings window) goes into a file of the user's own, `~/.config/mavernix/NAME.json`, which is laid over the config at the next start and which the shell watches.
3. **The shell and the compositor** talk over Wayfire's IPC socket: the shell asks for window pictures, desktop switches and the like, and the compositor's plugins report back.
4. **Switching desktop** while the session runs (`mavernix-switch NAME`, the settings window's Theme pane) runs `configure-desktop` again and restarts the shell; the compositor rereads its settings by itself.

## Where things are installed

| Path | What |
|---|---|
| `PREFIX/lib/mavernix/session/` | the session's scripts, with `paths.sh` and `paths.py`, which meson writes and which tell the scripts where everything else is |
| `PREFIX/lib/mavernix/bin/` | the desktop's own programs, put on the session's `PATH` |
| `PREFIX/lib/mavernix/wayfire/` | the compositor plugins |
| `PREFIX/lib/mavernix/qml/` | the shell's native module |
| `PREFIX/share/mavernix/` | the shell's QML, the programs' QML, configs, themes, looks, sounds, the plugins' option descriptions |
| `PREFIX/share/wayland-sessions/mavernix.desktop` | the login screen's entry |

## The user's files

| Path | What |
|---|---|
| `~/.config/mavernix/desktop` | which desktop starts |
| `~/.config/mavernix/NAME.json` | what the user changed of desktop NAME |
| `~/.config/mavernix/extensions/` | the user's extensions |
| `~/.local/share/mavernix/themes/` | the user's themes, the imported Sea Lion theme among them; looked at before the installed ones |
