# Configuration

Most things can be changed in the Settings window. This page is for everything else, and for when you want to know where your settings actually live.

## Desktops

A *desktop* is a config file. It names a theme and says how the desktop behaves.

Mavernix comes with one, `plain`: light and flat, with the keys you'd expect on Linux. It lives in `data/config/` and is installed into `/usr/share/mavernix/config/`. The Theme Installer adds the desktops for each look it builds to `~/.local/share/mavernix/config/`. For Sea Lion, those are `sea-lion` and `sea-lion-dark` (see [using the desktop](usage.md#the-sea-lion-look)).

If you haven't picked one, the desktop the Theme Installer added last starts. Before it has added any, that's `plain`.

To pick one, use the Theme pane in Settings. The change happens right away and is saved in `~/.config/mavernix/desktop`. In a terminal inside the session, `mavernix-switch NAME` does the same thing.

Switching rewrites everything that depends on the desktop and restarts the panel and dock, and the compositor redraws every window frame. Programs that are already open keep their old look until you start them again. That's because a toolkit only reads its theme when a program starts.

### A desktop of your own

Copy one of the config files into `~/.local/share/mavernix/config/` under a new name. You can also start one directly with `start-desktop /path/to/my.toml`. `plain.toml` explains each key whose meaning isn't obvious.

You don't have to copy everything. A config that says `based_on = "NAME"` is that desktop with your own keys laid on top. So a tiny file can change just one thing about a desktop you like.

## What you change while using it

Some things you change as you go: which programs are in the dock, its size, magnification, and anything you set in the Settings window. These aren't written into the config. They go into a file of your own, `~/.config/mavernix/NAME.json`, which is laid over the config every time the desktop starts.

Want a desktop's defaults back? Delete that file.

A desktop based on another one shares its file. So whatever you set under one stays when you switch to the other.

## Keys and gestures

In the config, keys are under `[shortcuts]` and `[commands]`, touchpad gestures under `[gestures]`, and the touchpad itself under `[input]`. The Keyboard pane in Settings changes keys without you having to edit any files.

Closing and quitting work like this:
- `close_window` closes the front window.
- `quit_app` closes every window of the front program. It has no key unless you give it one.
- With `[window] quit_closes_app = true` (Settings, Windows), the `close_window` key and Quit in the menu bar close all of the program's windows, as on OS X.

Two keys belong to the compositor, so they work even when nothing else does: Super+Enter opens a terminal, and Ctrl+Alt+Backspace ends the session right away. [Using the desktop](usage.md) lists the keys for each desktop.

The keyboard layout is your system's (see `localectl status`) until you pick one in Settings, Keyboard, Input Sources.

## Screens

A high-density screen is drawn at twice its pixels, any other at its normal size. To change that, set `[display] scale`, or use Settings, Displays while the desktop runs.

Settings, Displays also picks each screen's resolution and refresh rate. It saves them as `[display.screens.NAME] mode = "1920x1080@60000"`, with the refresh rate in millihertz. When you try a new mode, it switches back after 15 seconds unless you choose to keep it.

A screen built into the computer (eDP, LVDS, DSI) often reports only its own size. For those, Settings also offers the usual smaller sizes of the same shape, at the screen's own refresh rates. The graphics card stretches them to fill the screen, so they look a bit less sharp, but programs and games have fewer pixels to draw. Wayfire sets such a size from timings the session writes next to it (`custom_mode`, the way `cvt -r` makes them).

With more than one screen, the panel and dock go on the screen named in `[display] main_screen`, or on the first one if that isn't set.

To turn a screen off, pick it in Settings, Displays and untick Use this screen. That's saved as `[display.screens.NAME] enabled = false`. Screens that are off are listed under the picture, each with a button to turn it back on. One screen always stays on.

X11 programs (Steam, games under Proton or Wine, most SDL games) treat that same screen as their main one: they open on it and offer its resolutions. Without this, Xwayland names no main screen and games pick whichever one comes first. To turn it off, set `[display] x11_primary = false`.

Many games, and Wine or Proton in "borderless window" mode, open a window without a frame that covers the whole screen, instead of asking to be full screen. Mavernix makes such a window full screen on that screen, as KWin and GNOME do. Without that, the compositor would fit it between the menu bar and the Dock, and the game would be cut short. To turn this off, untick it in Settings, Windows, or set `[window] borderless_full_screen = false`.

A screen that supports variable refresh rate (VRR, also sold as FreeSync or G-Sync Compatible) gets a choice in Settings, Displays. It's saved as `[display.screens.NAME] vrr`:

- `"off"`, the default
- `"games"`: on only while a full-screen window shows on that screen. That's how games run, and the desktop doesn't flicker on screens that do so when their refresh rate drops.
- `"always"`

## Middle-click paste

By default, selecting text and pressing the middle mouse button pastes it, as on other Linux desktops. To turn that off everywhere, untick it in Settings, Mouse, or set `[input] middle_click_paste = false`. It takes effect the next time you log in. X11 programs still paste among themselves.

## Wayfire's own settings

The session writes Wayfire's settings file anew at every login and on every change, so editing it by hand doesn't last. Put your own Wayfire settings in `~/.config/mavernix/wayfire.ini` instead. It's added after everything the session writes, and Wayfire takes the last value it reads, so yours win. For example:

```ini
[output:DP-1]
vrr = true
```

A mistake there can break the session, and these settings aren't supported the way the desktop's own are. If something goes wrong, move the file away first.

## Locking

These lock the screen:

- Ctrl+Super+L on Sea Lion, Super+L on the others
- going to sleep
- `loginctl lock-session`
- ten minutes without input. Change the time under `[idle]` in the config, or in Settings, Energy Saver.

`[idle] sleep_after_seconds` puts the computer to sleep after a while without input. It's 0, meaning never, unless you set it.

`[power] lid_closed` decides what closing the lid does:

- `"sleep"` follows your system's own rule (logind's `HandleLidSwitch`).
- `"lock"` locks the screen. Like logind, it leaves a docked laptop alone, meaning one connected to a dock or a second screen, as logind counts it.
- `"nothing"` does nothing.

With `[power] lid_turns_off_screen = true` (Settings, Energy Saver), closing the lid while another screen is connected turns the built-in screen off, the way a Mac works with its lid shut. Opening the lid turns it back on. Whether the computer sleeps when the lid closes still depends on `lid_closed`, and on logind, which by default leaves a laptop with a second screen awake.

`"lock"` and `"nothing"` need UPower. Settings, Energy only offers the choice on a computer that UPower reports a lid for.

Before you rely on locking, try it once with the desktop running in a window inside another desktop (see [building](building.md#try-it-in-a-window)). The keys that end the session don't work while the screen is locked.

## Programs started at login

The programs in the config's `[session] autostart` start together with the desktop. So do the entries in `~/.config/autostart` and in your system's autostart folders (`/etc/xdg/autostart`), started through `dex`, unless you set `[session] users_autostart = false`.

Settings, Startup lists them all. Unticking one writes a copy into `~/.config/autostart` that says `Hidden=true`. Add… copies an installed program's entry there.

## The panel's items

The icons at the right end of the panel show what your computer's own services report. If a service isn't running, its icon simply takes up no room.

| Item | Service | Arch package |
|---|---|---|
| sound | PipeWire | `pipewire`, `wireplumber` |
| network | NetworkManager | `networkmanager` |
| Bluetooth | BlueZ | `bluez` |
| battery | UPower | `upower` |

Which items show up, and in what order, is set by `right` under `[panel]`. [Panel items](panel-items.md) explains how to add your own.

## The dock

`[dock] groups` lists what the dock holds, group by group, with a separator between groups. A place is a program's desktop entry ID, such as `"foot"`, or a table:

```toml
[dock]
groups = [
    ["foot", { name = "App Grid", icon = "view-app-grid", shell = "app_grid" }],
    [
        { name = "Downloads", icon = "folder-download", folder = "~/Downloads", view = "fan", order = "added" },
        { name = "Programs", icon = "folder", programs = true, view = "grid" },
    ],
]
```

- `shell` runs one of the shell's actions (see [the desktop's menus](#the-desktops-menus)), `command` runs a command.
- `folder` shows what's in a folder, as a `"fan"`, a `"grid"` or a `"list"`, sorted by `"name"`, `"added"`, `"modified"`, `"created"` or `"kind"`.
- `programs = true` is a folder of every installed program, shown like any folder, as a `"fan"`, a `"grid"` or a `"list"`. Its `order` is the same as a folder's: by `"added"` the newest programs come first, and by `"kind"` they're grouped by their main category. Its `subfolders`, each `{ name, icon, categories }`, gather the programs of those categories into a folder inside it, such as Sea Lion's Utilities for the `System` category; in a grid it opens in place with a back button, in a list as a submenu. A grid with more programs than fit on the screen scrolls. Its last entry opens the app grid. Both desktops have one. If you removed it, drag a folder of launchers, such as `/usr/share/applications`, onto the dock to get it back.

Once you've changed the dock by hand, your own groups in `NAME.json` replace the config's, so a new place in the config only shows up after you delete that file.

Right-clicking a program in the dock also lists the program's own entries from its launcher, such as New Window or New Private Window. `[dock] actions_when_closed = false` shows them only while the program runs.

A click on a program's icon brings it to the front, as on OS X. With `[dock] click_minimizes = true` (Settings, Dock & Menu Bar), a click on the icon of the program already in front minimizes its front window instead.

## Applications' icons and names

Settings, App Icons & Names, gives an application another icon or another name, for example Bandstand the name "iTunes". The desktop then shows them instead of the application's own: in the dock, the menu bar, the app grid, the program switcher, the window overview and the search. The search still finds it by its own name too. What the application shows itself, like its window title or its About box, stays its own.

Both are kept by desktop entry ID, so they belong to a desktop like everything else you change (a desktop based on another one shares them):

```toml
[app_icons]
"bandstand" = "/home/you/Pictures/itunes.png"   # a picture, or an icon theme's name

[app_names]
"bandstand" = "iTunes"
```

An empty value, or Use Original, brings back the application's own.

To take an application out of the app grid, right-click it there and choose Hide from the App Grid, or untick Show in the app grid in Settings, App Icons & Names. That's also where you bring it back. The search and the dock's Programs folder still list it. It's saved as `[app_grid] hidden = ["htop", ...]`, by desktop entry ID.

## The desktop's menus

When no window is active, the panel shows the desktop's own name and menus, just like a program would. That's where things like New Folder, Go to Home or Downloads, and Empty Wastebasket live. They're set up under `[desktop_menus]`:

```toml
[desktop_menus]
name = "Desktop"

[[desktop_menus.menus]]           # the first menu opens from the name itself, so it has no title
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

An entry either runs a `command` or one of the shell's `action`s. In a command, a leading `~` means your home folder, and adding a `confirm` table makes it ask first, like the logo menu does. The actions are `app_grid`, `window_overview`, `search`, `notification_list`, `lock` and `empty_trash`.

Without a `[desktop_menus]` table, the panel stays empty while no window is active.

You can rename the desktop in Settings, Desktop ("Name in the menu bar"), for example to "Finder". The first menu stays under the name.

## About This Computer

`mavernix-about` opens a small window that tells you about your computer: its processor, memory, startup disk, graphics and model, plus the system's name and version. You'll find it in plain's Desktop menu, and in Sea Lion's logo menu as "About This PC".

What it shows, and every word in it, is set under `[about]`. How it looks is up to the theme's `[about]` (see `docs/themes/programs.md`).

```toml
[about]
name = "About This Computer"           # the window's title, and its name in the dock
heading = "{os_name}"                  # in large type, when the theme has no picture
# Shown below it, one at a time. Clicking shows the next one. A line that needs
# a fact the computer doesn't tell us is left out.
version_lines = ["Version {os_version}", "Kernel {kernel}", "{host_name}"]
rows = ["processor", "memory", "startup_disk"]
more_rows = ["graphics", "computer"]   # shown by the second button while it has no program
keep_idle_buttons = false              # true keeps a button with nothing to start, greyed out
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

# To set a fact yourself instead of asking the computer:
# [about.values]
# computer = "My own build"
```

These are the facts you can put in braces: `{os_name}`, `{os_version}`, `{kernel}`, `{host_name}`, `{computer}`, `{processor}`, `{memory}`, `{graphics}`, `{startup_disk}`. They're read from files any user can read (`/proc`, `/sys`, udev's notes, `/etc/os-release`, and the `hwdata` list of graphics cards). Run `mavernix-about --facts` to see what it finds.

## App Store

`mavernix-app-store` installs apps made for Mavernix, such as Bandstand. It finds them on GitHub: every repository of an organisation that carries a certain topic is an app, and its latest release brings what the store shows and installs. Apps are installed for you alone, in `~/.local/share/mavernix/apps/`, so no password is needed. How to publish an app is in [app-store.md](app-store.md). To make its window's title bar run on into its toolbar, list `mavernix-app-store` in `joined_tool_bar_apps`, as Sea Lion does.

Where it looks, and every word it shows, is set under `[app_store]`. How it looks is up to the theme's `[app_store]` (see `docs/themes/programs.md`).

```toml
[app_store]
name = "App Store"                     # the window's title, and its name in the app grid and the dock
organisation = "Mavernix"              # the GitHub organisation whose apps it offers
topic = "mavernix-app"                 # the topic that makes a repository an app
extensions_repository = "mavernix-extensions"   # the repository whose releases offer extensions; empty for none

[app_store.words]
featured = "Featured"                  # the tabs
installed = "Installed"
updates = "Updates"
search = "Search"                      # in the empty search field
results = "Results for “{text}”"
no_results = "No apps match “{text}”."
featured_heading = "Apps for Mavernix"
get = "GET"                            # the button: GET, then INSTALL to confirm, then OPEN
install = "INSTALL"
open = "OPEN"
update = "UPDATE"
update_all = "UPDATE ALL"
remove = "Remove"
waiting = "WAITING"                    # while another app installs first
installing = "INSTALLING"
removing = "REMOVING"
whats_new = "What's New in Version {version}"
information = "Information"            # the column on an app's page, and its rows
category = "Category"
released = "Updated"
version = "Version"
size = "Size"
developer = "Developer"
license = "License"
website = "Website"
loading = "Loading…"
try_again = "Try Again"
nothing_offered = "There are no apps in the App Store yet."
nothing_installed = "Apps you get from the App Store show up here."
no_updates = "No Updates Available"
one_update = "1 Update Available"
updates_available = "{count} Updates Available"
```

## Living next to another desktop

Some settings are stored per user rather than per desktop. Without some care, another desktop's choices would show up here too: Plasma's dark colours, its icons and font in GTK programs, KDE's background modules and their notifications. So the session keeps its own copies, without touching the other desktop's:

- **GTK's settings store** (dconf): the session has its own database, `~/.config/dconf/mavernix`, next to the usual `user` one. The theme's light or dark mode, icon theme and font are written into it every time the desktop starts.
- **A settings folder for the session**, listed in `XDG_CONFIG_DIRS` after your own. It holds the desktop portal's choice of helpers, and a `kded6rc` that switches off Plasma's own modules in KDE's background service while you're here. Your own settings for a module still win.
- **KDE's wallet**: when the login screen started the wallet, the session hands it your login, so it doesn't ask for your password.

Services that start on demand learn about all of this at login. When the session ends, a small service takes it all back out, so a Plasma session you start next doesn't inherit any of it.

## Background blur for the menu bar and Dock

The Desktop pane has a **Translucent menu bar** checkbox, and the Dock & Menu
Bar pane has **Blur menu bar** and **Blur behind the Dock**, all off at first:
the menu bar then shows the theme's picture of it as it is. Both blurs are
extras Mavericks didn't have (`blur.menu_bar`, `blur.dock_shelf`). The menu
bar's blur only shows while it is translucent; turning translucency off
restores the original material and keeps the blur choice for later. The
Dock's blur doesn't depend on the menu bar's translucency.
`blur.panel` and `blur.dock` only say that a surface can blur; on their own
they blur nothing, so older imports that set them stay unblurred.
It uses Wayfire's `blur` plugin with an explicit matcher for `desktop-panel`
and/or `mavernix-dock`; ordinary windows are excluded.

```toml
[blur]
panel = true # the menu bar can blur
dock = true # the Dock's shelf can blur
menu_bar = false # Blur menu bar, in the Dock & Menu Bar pane
dock_shelf = false # Blur behind the Dock, in the Dock & Menu Bar pane
panel_opacity = 0.65 # opacity of the menu bar's background, not its contents
# The 3D shelf already has translucent colours; keep its original finish.
dock_opacity = 1.0
method = "gaussian" # kawase, gaussian, box or bokeh
iterations = 2 # whole number, 0..10
degrade = 1 # whole number, 1..10
offset = 1.5 # 0..25
saturation = 1.0 # 0..3
alpha_threshold = 0.5 # 0.01..1
```

An opaque background needs reduced opacity for the blur to show through.
Only the shelf/background opacity changes; text, icons, highlights and the
shelf geometry remain sharp. The compositor uses surface alpha to blend the
blurred background, including the shaped shelf's edges. Blur requires Wayfire's
GLES renderer and its matching `blur` plugin. It adds GPU work; switching it off
for both surfaces unloads the plugin. Changing a blur preference briefly restarts
the desktop shell to remap the surfaces with the new matcher.
