# Changelog

## Unreleased

- The Theme Installer accepts installers of other OS X 10.9 versions: only the artwork files it reads by position must be 10.9.5's, not the CoreUI library, whose colours are found by name.
- Switching desktop while the session runs (the Theme Installer's Use button, Settings' Theme pane) wrote the new desktop but left the old panel and dock running until the next login: the shell inherited Wayfire's ignored SIGHUP.
- Qt programs got Kvantum's own dark look instead of the theme's: Kvantum does not look for themes through `XDG_DATA_DIRS`, so the session links its theme into `~/.config/Kvantum/`.
- Programs started on demand by D-Bus or systemd, such as Thunar's daemon, got no theme or icons: the session now hands them the look's environment and `XDG_DATA_DIRS`, updates it on a switch, and takes it back after the session.
- The Theme Installer no longer freezes while it checks the computer, reads a dropped installer or switches desktop: each runs beside the window, with a busy bar, and reading an installer can be stopped. Use waits for the switch and says why it failed.
- The Theme Installer reads the 5.3 GB installer image twice instead of eight times (its list of files is kept, and the parts come out in one pass); it needs 5 GB free while it runs instead of 4.5. One dated log holds the whole import, the sandboxed part included.

## 0.1.1 (2026-10-25)

- The package brings what a fresh system lacks: the plain theme's icons and font, and the Theme Installer's tools; a missing icon theme falls back to an installed one.
- foot is optional: the package no longer installs a terminal of its own.
- The keyboard layout test passes in every locale, not only in C, so `makepkg` no longer stops at it.
- Plainer wording in the Theme Installer and the docs.

## 0.1.0


The first public release.

- A desktop session on Wayfire and Quickshell: panel with the program's menus and status items, dock with magnification and folders, desktop icons, menus, notifications and their list, program switcher, app grid, window overview, search, lock screen, several desktops.
- Compositor plugins: window frames drawn from the theme, minimise effects, window pictures for the dock, touchpad gestures, tiling keys, shared edges between tiled windows.
- A settings window.
- The `plain` theme, and a theme format documented in `docs/themes/`; a theme can build on another (`based_on`), and `mavernix-check-theme` holds a theme to the format.
- Toolkit themes for Qt (through Kvantum) and GTK made from the desktop theme.
- File actions on the desktop through GLib's `gio`; folders open in the system's own file manager.
- The desktop's own menus in the panel while no window is active (`[desktop_menus]`): new folder, the usual folders, the app grid, emptying the wastebasket.
- The Theme Installer: makes a look's themes and desktops from an installer the user owns, each look a folder of data (`backend/looks/`), so far Sea Lion; a dropped file is recognised at once, and the first page shows which looks are installed (checked file by file: the artwork files it decodes must be 10.9.5's, other pictures and sounds may differ), with a window that shows each step; its launcher is put on the desktop at the first login.
- Program launchers on the desktop: an executable `.desktop` file starts its program and takes files dropped on it.
