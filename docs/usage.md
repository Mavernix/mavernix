# Using the desktop

"Super" is the key with the Windows logo. Keys differ between the desktops: the first table is for `plain`, the second for `sea-lion`. Every key can be changed in Settings, Keyboard.

## Where things are

| What | Where |
|---|---|
| Programs you keep | the dock at the bottom; a click starts or shows one, a right click has "Keep in Dock" and "Quit" |
| All programs | the app grid: its tile in the dock, its key, or a four-finger pinch |
| Open windows side by side | the window overview: its key, or a three-finger swipe up |
| Programs and files by name | search: the magnifying glass in the panel, or Super+Space |
| Past notifications | the icon at the panel's right end |
| Settings | the menu at the panel's end, "Settings…", or the dock's right-click menu |
| Files | the Home tile in the dock opens your home folder; folders open in the system's own file manager |
| A folder's contents | its tile in the dock (Downloads at first); a right click changes how it opens |
| Log out, restart, shut down | the menu at the panel's end; each asks first |

## Keys

| | `plain` | `sea-lion` |
|---|---|---|
| search | Super+Space | Super+Space |
| all programs | Super+A | F4 |
| all windows | Super+W | F3, or Ctrl+Up |
| show the desktop | Super+D | F11 |
| program switcher | Super+Tab | Super+Tab |
| next window | Alt+Tab | Super+` |
| close the window | Alt+F4 | Super+Q |
| minimise | Super+H | Super+M |
| full screen | F11 | Ctrl+Super+F |
| left or right half, then a quarter; whole screen; back | Super+arrows | Super+arrows |
| the desktop at the left or right | Ctrl+Alt+Left, Right | Ctrl+Left, Right |
| picture of the screen, of a part | Print, Shift+Print | Super+Shift+3, Super+Shift+4 (with Ctrl: into the clipboard) |
| lock | Super+L | Ctrl+Super+L |
| a terminal | Super+Enter | Super+Enter |
| end the session at once | Ctrl+Alt+Backspace | Ctrl+Alt+Backspace |

The program switcher works as on other desktops: hold Super, press Tab until the program you want is marked, let go. Shift goes backwards; a quick tap goes to the program used before.

## Touchpad

| Gesture | What it does |
|---|---|
| three fingers up | all windows side by side |
| four fingers together | all programs |
| four fingers apart | show the desktop |
| three fingers left or right | the next desktop |

Under `sea-lion`, tapping does not click and scrolling is "natural"; both are in Settings, Trackpad. If a gesture fires too easily or not at all, the distances are `swipe_distance` and `pinch_change` under `[gestures]` in the config.

## Windows

- Drag a window by its title bar to the screen's left or right side and it takes that half; into a corner, that quarter; to the top, the whole screen. Drag it away again and it has its old size.
- Super with the arrow keys goes on from where the window is: Left puts it on the left half, then Up on the upper left quarter.
- Two tiled windows that lie against each other share their edge: pull it and one grows as the other shrinks.
- Hold Super and drag anywhere on a window to move it; with the right button, to resize it.
- A double click on a title bar makes the window as large as the screen, and back.

All of this can be changed under `[window]` in the config, or in Settings, Windows.

## Several desktops

Open the window overview. Along its top are your desktops; move the pointer into the top right corner and click the tile with the plus to add one. Click a desktop to go there. To take a window to another desktop, drag its picture onto the desktop's picture, or drag the window itself to the screen's side and hold it there a moment. A desktop's round button removes it; its windows move to the desktop before it.

## The desktop and the dock

- The files of your Desktop folder show on the desktop. Drag them anywhere; "Clean Up" in the right-click menu puts them back on the grid.
- Drag a tile along the dock to move it, or up and out of it to remove it. Drag a file or folder from the file manager onto the dock to keep it there.
- Drag files onto the Trash in the dock to throw them away; its right-click menu empties it.
- The dock can hide until the pointer reaches the screen's bottom edge: Settings, Dock.
- A minimised window sits at the dock's end as a small picture of itself; click it to bring it back.

## The Sea Lion look

Sea Lion recreates the desktop of OS X 10.9 Mavericks: window frames, panel and dock, menus, icons, sounds and desktop pictures, in a light and a dark version. None of it is part of Mavernix; the **Theme Installer** makes it on your computer from your own copy of the OS X installer, and keeps it for your user only. Nothing is downloaded, and nothing is sent anywhere. Please don't share the themes it makes: they contain Apple's artwork.

Supported installers:

| OS X | Build | What you can give the Theme Installer |
|---|---|---|
| 10.9.5 | 13F34 | `Install OS X Mavericks.app`, the `InstallESD.dmg` inside it, or a disk image (`.dmg`, `.iso`) made from either |
| 10.9 to 10.9.4 | | untested: they work if the artwork files Sea Lion decodes are the same as 10.9.5's, and are refused otherwise ([looks](looks.md) says how a version is added) |

Modified installers, such as those of hackintosh distributions, are refused.

1. Open the Theme Installer. After your first login its icon is on the desktop (you can delete it; it is also in the app grid).
2. Drop the installer on its icon or into its window, or choose it there. A copy whose artwork files differ from 10.9.5's is refused; if only some pictures or sounds differ, the theme is made from them as they are, and the window says how many.
3. The import takes a few minutes and about 4.5 GB of free space while it runs. The window shows each step and how far it is.
4. When it is done, "Use Sea Lion" switches to the Sea Lion desktop. Settings, Theme switches back and forth later.

The import needs a few tools, which the Theme Installer names if one is missing: `7zip`, `cpio`, `bzip2`, `libicns`, `ffmpeg`, `poppler` and `python-pillow`. With `bubblewrap` installed, it reads the installer in a sandbox without network access; with `lbzip2` installed, it unpacks on every processor core, which makes it several times faster. The themes end up in `~/.local/share/mavernix/themes/`; importing again replaces them.

## Settings

The settings window opens on a page with every pane as an icon; type a pane's name into its search field to go straight there. It covers the dock, the desktop's picture and which desktop it is, displays, sound, Bluetooth, the trackpad, keyboard layouts and shortcuts, default programs, energy saving, windows, several desktops, and each program's icon. Changes take effect at once. What the window does not cover is in the desktop's config; see [configuration](configuration.md).

## If something goes wrong

- Nothing reacts: Ctrl+Alt+Backspace ends the session.
- The screen is locked and your password is not taken: Ctrl+Alt+F3 for a text console, log in there, then `loginctl unlock-session` with the session's number from `loginctl`, or `pkill wayfire` to end the desktop.
- The panel or the dock is gone: they restart by themselves within a few seconds; if they keep failing, a notice says so.
- If the desktop cannot start at all, a notice says why, and a key takes you back to the login screen.
- Logs: `$XDG_RUNTIME_DIR/mavernix/session.log` (the compositor) and `login.log` (the shell and what was started at login).
