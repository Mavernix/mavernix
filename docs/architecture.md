# How the desktop is put together

Mavernix is built from existing parts. [Wayfire](https://github.com/WayfireWM/wayfire) is the compositor, [Quickshell](https://quickshell.org) runs the shell, and Qt draws the desktop's own programs. Our code adds what none of them has: the panel, dock and overlays, a few compositor plugins, the session that ties it all together, and a theme format that every part reads.

## Three layers

- **Code** builds the generic desktop parts: panel, dock, window frames, window overview, app grid, notifications, search. It knows nothing about any particular look.
- **A theme** (`data/themes/NAME/theme.toml` and its pictures) holds everything about how things look: sizes, colours, fonts, pictures, sounds, and the kind, duration and curve of every animation. See [the theme format](themes/README.md).
- **A desktop config** (`data/config/NAME.toml`, or one that comes with a look) holds behaviour and layout: which parts are shown and where, which side the window buttons go on, keys, gestures, and the programs in the dock. See [configuration](configuration.md).

One rule keeps these apart: you should be able to make a different desktop with a different theme and config, without touching the code. If there's something you can't express, the theme format or the config gets a new key. The code never learns about a specific theme.

## The parts

| Folder | What it is | Language |
|---|---|---|
| `src/session/` | starts the desktop: turns a config and its theme into settings for the compositor and the shell, sets up the environment, then runs Wayfire | shell scripts, Python |
| `src/compositor/` | Wayfire plugins: window frames drawn from the theme, the minimise effects and the pop of opening windows, pictures of single windows for the dock, touchpad gestures, keys that tile windows, shared edges between tiled windows, parts of the shell that stay put while desktops slide, sliding the desktop aside for the list of notifications, recording a shortcut | C++ |
| `src/shell/` | the panel, dock, desktop icons, menus, notifications, app grid, window overview, search, lock screen | QML, run by Quickshell |
| `src/shell/native/` | a QML module for things QML can't do: application menus over D-Bus, file kinds, icons, folder watching, the compositor's socket | C++ |
| `src/apps/settings/` | the settings window | C++, Qt Widgets |
| `src/apps/app-store/` | the App Store: a window over a back end (`backend/`) that finds the apps an organisation publishes on GitHub and installs them for the user ([publishing an app](app-store.md)) | C++ with QML; Python |
| `src/apps/theme-installer/` | the Theme Installer: a window on top of a back end (`backend/`) that makes a look's themes from an installer you own. Each look is a folder in `backend/looks/` ([looks](looks.md)) | C++ with QML; Python |
| `data/` | what gets installed and read: configs, themes, looks for other programs, the sound pack | TOML, pictures |
| `tests/` | the tests | C++, Python |
| `dev/` | trying the desktop in a container, plus small tools that made some of the data | shell, Python |

## How they talk

1. **At login**, `start-desktop` reads the chosen desktop config and finds its theme. Then `configure-desktop` writes the session's state into `$XDG_RUNTIME_DIR/mavernix/`:
   - `session.ini`: Wayfire's settings (`make_wayfire_config.py`). They load our plugins and tell them where the theme's folder is.
   - `shell.json`: the config and the theme, for the shell and the programs (`make_shell_config.py`).
   - toolkit themes for GTK and Qt programs, made from the theme's `[controls]` (`make_app_theme.py`).
   - desktop entries for the desktop's own programs, and the environment for everything started in the session.

   Then it becomes Wayfire, and Wayfire starts the shell.
2. **The shell and the programs** read `shell.json`. Its path is in `DESKTOP_SHELL_SETTINGS`. When you change something while the desktop runs (the dock's contents, anything in the settings window), it goes into your own file, `~/.config/mavernix/NAME.json`. That file is laid over the config at the next start, and the shell watches it.
3. **The shell and the compositor** talk over Wayfire's IPC socket. The shell asks for things like window pictures and desktop switches, and the compositor's plugins report back.
4. **Switching desktop** while the session runs (`mavernix-switch NAME`, or the Theme pane in the settings window) runs `configure-desktop` again and restarts the shell. The compositor rereads its settings on its own.

## Where things are installed

| Path | What |
|---|---|
| `PREFIX/lib/mavernix/session/` | the session's scripts, plus `paths.sh` and `paths.py`. Meson writes those two, and they tell the scripts where everything else is |
| `PREFIX/lib/mavernix/bin/` | the desktop's own programs, put on the session's `PATH` |
| `PREFIX/lib/mavernix/wayfire/` | the compositor plugins |
| `PREFIX/lib/mavernix/qml/` | the shell's native module |
| `PREFIX/share/mavernix/` | the shell's QML, the programs' QML, configs, themes, looks, sounds, the plugins' option descriptions |
| `PREFIX/share/wayland-sessions/mavernix.desktop` | the entry on the login screen |

## The user's files

| Path | What |
|---|---|
| `~/.config/mavernix/desktop` | which desktop starts |
| `~/.config/mavernix/NAME.json` | what the user changed in desktop NAME |
| `~/.config/mavernix/extensions/` | the user's extensions |
| `~/.local/share/mavernix/apps/` | the apps installed from the App Store, each with a desktop entry in `~/.local/share/applications/` |
| `~/.local/share/mavernix/themes/` | the user's themes, including the imported Sea Lion theme. These are checked before the installed ones |
