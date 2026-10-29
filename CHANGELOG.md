# Changelog

## Unreleased

- New Network pane in Settings. It lists every Wi-Fi network around you and the ones you've saved, with plain Connect, Disconnect and Forget buttons.
- The Wi-Fi menu in the menu bar now lists the networks around you, so you can join one, type its password or disconnect without opening Settings or installing anything else.
- New Mouse pane in Settings for tracking speed, scrolling speed, acceleration, scroll direction and which button is the main one.
- The Touchpad pane can now set tracking speed, acceleration and how a right-click works.
- Bluetooth keyboards, phones and other devices that ask for a PIN or a code can now be paired. Clicking a device in the Bluetooth menu connects or disconnects it.
- Clicking the desktop now makes the open window inactive, and the menu bar shows the desktop's own menus.
- Settings has a brightness slider for laptop screens, and volume sliders for your speakers and microphone.
- With more than one screen, Settings, Display lets you pick which screen has the menu bar and the dock, and where the others stand next to it. Your arrangement is no longer lost when you change other settings.
- You can choose which icons show in the menu bar, switch the clock to 24 hours, and hide the files on the desktop.
- Lock Screen is now in the menu bar's session menu, and in Sea Lion's Apple menu.
- You get a warning when the battery runs low, and the battery menu has a shortcut to the Energy settings.
- Notifications can be cleared all at once with Clear All, or one at a time.
- Right-clicking an app in the app grid lets you open it or keep it in the dock. Keep in Dock and Remove from Dock are now right in the dock's menu instead of under Options.
- Programs without a launcher of their own now show up in the dock while they run.
- When you drag a file over the dock, the icons make room where it will land.
- App icons that fill their whole square are shrunk a little in the dock, so they match the theme's own icons.
- Dialogs now show a greyed-out minimise button instead of none.
- In the window overview you can close a window from its picture, and add or remove desktops with any theme.
- Searching in Settings now finds panes by everyday words, like "wifi", "wallpaper" or "brightness".
- Window buttons and the double-click on a title bar are now set in Settings, Windows, and the side setting moves the buttons of every window.
- Sea Lion switches spaces with Ctrl+Alt+arrows, so Ctrl+arrows keep moving by words in text fields. Import the theme again to get it.
- Settings panes no longer get cut off or leave large empty areas, and the wording is plainer throughout.

## 0.1.5 (2026-10-27)

- New Startup pane in Settings (called Login Items in Sea Lion). It shows the programs that open when you log in. You can switch any of them off, add your own, or remove ones you added.
- Your computer can now go to sleep on its own after a while without use. Set it in Settings, Energy. It's off by default, and a playing video or a call keeps the computer awake.
- You can pick each screen's resolution and refresh rate in Settings, Display. If the screen goes dark, it switches back after 15 seconds unless you click Keep.
- Laptop screens usually offer just one size. Settings now also offers smaller sizes of the same shape, which look bigger and help with games. If you use Sea Lion, import the theme again to get the new labels.
- You can choose what closing the lid does in Settings, Energy: sleep, lock the screen, or nothing. When the laptop is plugged into a dock or another screen, closing the lid leaves it alone, as before.
- KDE apps like Konsole now follow the desktop's colours. Before, they could show dark text on white under a dark theme.
- Moving an Energy slider with the arrow keys could freeze the desktop. Fixed.

## 0.1.4 (2026-10-25)

- Sea Lion Dark's About This PC shows "OS X" under the logo in light grey; it was the light theme's dark grey, nearly lost on the dark window. Import the theme again to get the new picture.
- The battery menu's Show Percentage is kept between sessions, in the user's choices, instead of going back to the config's setting at the next login.

## 0.1.3 (2026-10-25)

- The desktop's name in the menu bar ("Desktop") can be changed in Settings, Desktop, for example to "Finder"; it changes at once, and the desktop's first menu stays under it.
- Sea Lion Dark's tool bar buttons (Settings' back, forward and Show All) have a bezel like the light theme's, from ProKit's dark segmented button, instead of Kvantum's flat stock look.
- A window whose program fixes its size, such as Settings, can no longer be resized by its edges or maximized: no resize pointer at its edges, and its maximize button is disabled. Themes may draw a disabled button (`disabled`); Sea Lion's are Apple's own.

## 0.1.2 (2026-10-25)

- Mouse pointers as part of a theme (`[cursors]`), for the compositor, GTK and other programs; Sea Lion's are made from the installer's own (the arrow, text beam and spinning wait cursor from CoreGraphics, the rest from HIServices' drawings).
- An About This Computer window (`mavernix-about`), in plain's Desktop menu and as "About This PC" in Sea Lion's logo menu: Sea Lion's is laid out as the original's, with its picture and small buttons, plain's is the system's name in large type; the processor, memory, startup disk, graphics and model, from files any user may read.
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
