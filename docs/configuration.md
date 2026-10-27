# Configuration

## Desktops

A *desktop* is a config file that names a theme and says how the desktop behaves. `plain`, a light, flat desktop with the keys usual on Linux, comes with Mavernix (`data/config/`, installed in `/usr/share/mavernix/config/`). The Theme Installer adds the desktops of each look it makes to `~/.local/share/mavernix/config/`; for Sea Lion those are `sea-lion` and `sea-lion-dark` (see [using the desktop](usage.md#the-sea-lion-look)).

Without a choice, the desktop the Theme Installer added last starts, and `plain` before any was added. The choice is made in the settings window's Theme pane; it takes effect at once, and is kept in `~/.config/mavernix/desktop`. From a terminal inside the session, `mavernix-switch NAME` does the same.

Switching rewrites everything that depends on the desktop and restarts the panel and dock; the compositor redraws every window frame. Programs that are already open keep their look until they are started again, because a toolkit reads its theme when a program starts.

### A desktop of your own

Copy one of the files under another name into `~/.local/share/mavernix/config/` (or start one directly: `start-desktop /path/to/my.toml`). `plain.toml` explains each key whose meaning is not obvious. A config that says `based_on = "NAME"` is that desktop with its own keys laid over it, so a small file can change one thing about an existing desktop.

## What you change while using it

The dock's programs, its size, magnification and everything set in the settings window are not written into the config but into a file of your own, `~/.config/mavernix/NAME.json`, which is laid over the config at every start. Delete the file to get a desktop's defaults back. A desktop based on another shares its file: what you set under the one stays when you switch to the other.

## Keys and gestures

Keys are `[shortcuts]` and `[commands]` in the config, touchpad gestures `[gestures]`, the touchpad itself `[input]`. The settings window's Keyboard pane changes keys without editing files. Two keys belong to the compositor and work even when nothing else does: Super+Enter opens a terminal, and Ctrl+Alt+Backspace ends the session at once. [Using the desktop](usage.md) lists the keys of each desktop.

The keyboard layout is the system's (`localectl status`) until one is chosen in Settings, Keyboard, Input Sources.

## Screens

A dense screen is drawn at twice its pixels, any other at once; `[display] scale` says otherwise, and Settings, Displays changes it while the desktop runs. Settings, Displays also chooses each screen's resolution and refresh rate, written as `[display.screens.NAME] mode = "1920x1080@60000"` (the refresh rate in millihertz); a new mode goes back after 15 seconds unless it is kept. A screen built into the computer (eDP, LVDS, DSI) that reports only its own size is offered the usual smaller sizes of the same shape too, at its own refresh rates: the graphics card stretches them to the panel, so they look less sharp, but programs and games draw fewer pixels. With more than one screen, the panel and dock are on the one `[display] main_screen` names, else on the first.

## Locking

Ctrl+Super+L under Sea Lion, Super+L under the others, locks the screen; so does going to sleep, `loginctl lock-session`, and ten minutes without input (`[idle]` in the config, or Settings, Energy Saver). `[idle] sleep_after_seconds` puts the computer to sleep after a time without input; it is 0, never, unless set. `[power] lid_closed` says what closing the lid does: `"sleep"` (the system's own rule, logind's `HandleLidSwitch`), `"lock"` or `"nothing"`; the last two need UPower, and Settings, Energy offers the choice only on a computer UPower reports a lid for. Try it once while the desktop runs in a window inside another one (see [building](building.md#try-it-in-a-window)) before relying on it: the keys that end the session do not work while locked.

## Programs started at login

The config's `[session] autostart` starts its programs with the desktop. Unless `[session] users_autostart = false`, so do the entries in `~/.config/autostart` and the system's autostart folders (`/etc/xdg/autostart`), through `dex`. Settings, Startup lists them: unticking one writes a copy into `~/.config/autostart` that says `Hidden=true`, and Add… copies an installed program's entry there.

## The panel's items

The icons at the panel's right end show what the machine's own services report, and take no room when a service is not running:

| Item | Service | Arch package |
|---|---|---|
| sound | PipeWire | `pipewire`, `wireplumber` |
| network | NetworkManager | `networkmanager` |
| Bluetooth | BlueZ | `bluez` |
| battery | UPower | `upower` |

Which items are shown, and in what order, is `right` under `[panel]`. [Panel items](panel-items.md) says how to add one of your own.

## The desktop's menus

While no window is active, the panel shows the desktop's own name and menus, as a program would: New Folder, Go to Home or Downloads, Empty Wastebasket and the like. They are `[desktop_menus]`:

```toml
[desktop_menus]
name = "Desktop"

[[desktop_menus.menus]]           # the first menu opens from the name itself: no title
entries = [
    { label = "About This Computer", command = ["mavernix-about"] },
    { label = "Settings…", command = ["mavernix-settings", "desktop"] },
    { separator = true },
    { label = "Empty Wastebasket…", action = "empty_trash" },
]

[[desktop_menus.menus]]
title = "Go"
entries = [
    { label = "Home", command = ["gio", "open", "~"] },
    { label = "Applications", action = "app_grid" },
]
```

An entry runs a `command` (a leading `~` is your home folder; with a `confirm` table it asks first, as the logo's menu does) or one of the shell's `action`s: `app_grid`, `window_overview`, `search`, `notification_list`, `lock`, `empty_trash`. Without the table the panel stays empty while no window is active.

The name can be changed in Settings, Desktop ("Name in the menu bar"), for example to "Finder"; the first menu stays under it.

## About This Computer

`mavernix-about` opens a small window that says what the computer is: its processor, memory, startup disk, graphics and model, and the system's name and version. Plain's Desktop menu opens it, and Sea Lion's logo menu as "About This PC". What it shows and every word are `[about]`; how it looks is the theme's `[about]` (`docs/themes/programs.md`).

```toml
[about]
name = "About This Computer"           # the window's title, and its name in the dock
heading = "{os_name}"                  # in large type, where the theme has no picture
# Under it, one at a time; a press shows the next. A line that asks for a
# fact the computer does not tell is left out.
version_lines = ["Version {os_version}", "Kernel {kernel}", "{host_name}"]
rows = ["processor", "memory", "startup_disk"]
more_rows = ["graphics", "computer"]   # shown by the second button while it has no program
keep_idle_buttons = false              # true keeps a button with nothing to start, greyed
# update_command = ["…"]               # what the first button starts
# more_command = ["…"]                 # what the second button starts
footer = ["Mavernix"]

[about.words]
processor = "Processor"
memory = "Memory"
startup_disk = "Startup Disk"
graphics = "Graphics"
computer = "Computer"
update = "Update…"
more = "More…"

# A fact fixed instead of asked of the computer:
# [about.values]
# computer = "My own build"
```

The facts, in braces in the lines: `{os_name}`, `{os_version}`, `{kernel}`, `{host_name}`, `{computer}`, `{processor}`, `{memory}`, `{graphics}`, `{startup_disk}`. They are read from files any user may read (`/proc`, `/sys`, udev's notes, `/etc/os-release`, the `hwdata` list of graphics cards); `mavernix-about --facts` prints what it finds.

## Living beside another desktop

Some settings are kept per user, not per desktop, so another desktop's choices (Plasma's dark colours, its icons and font in GTK programs, KDE's background modules and their notifications) would show here too. The session keeps its own, without touching the other desktop's:

- **GTK's settings store** (dconf): the session has a database of its own, `~/.config/dconf/mavernix`, beside the usual `user`. The theme's light or dark, icon theme and font are written into it at every start.
- **A settings folder of the session**, listed in `XDG_CONFIG_DIRS` after your own: the desktop portal's choice of helpers, and a `kded6rc` that switches off Plasma's own modules of KDE's background service here. Your own settings for a module still win.
- **KDE's wallet**: when the login screen started it, the session hands it the login, so that it does not ask for the password.

Services started on demand learn of these at login, and a small service takes them back out when the session ends, so that a Plasma session started next does not inherit them.
