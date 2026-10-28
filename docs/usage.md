# Using the desktop

Here's how to get around. "Super" is the key with the Windows logo on it.

The keys aren't the same on every desktop, so the table below has one column for `plain` and one for `sea-lion`. Don't like a key? You can change any of them in Settings, Keyboard.

## Where things are

| What | Where |
|---|---|
| Your programs | The dock at the bottom. Click a program to start it, or to bring it up if it's already open. Right-click it for "Keep in Dock" and "Quit". |
| All programs | The app grid. Open it from its tile in the dock, with its key, or with a four-finger pinch. |
| All open windows side by side | The window overview. Use its key, or swipe up with three fingers. |
| Find a program or file by name | Search. Click the magnifying glass in the panel, or press Super+Space. |
| Notifications you missed | The icon at the right end of the panel. |
| Settings | "Settings…" in the menu at the end of the panel, or in the dock's right-click menu. |
| Your files | The Home tile in the dock opens your home folder. Folders open in your system's usual file manager. |
| What's in a folder | Click its tile in the dock (Downloads is there to start with). Right-click it to change how it opens. |
| Log out, restart, shut down | The menu at the end of the panel. It always asks first. |

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
| the desktop to the left or right | Ctrl+Alt+Left, Right | Ctrl+Left, Right |
| screenshot of the screen, of a part | Print, Shift+Print | Super+Shift+3, Super+Shift+4 (add Ctrl to copy it instead) |
| lock | Super+L | Ctrl+Super+L |
| a terminal | Super+Enter | Super+Enter |
| end the session right away | Ctrl+Alt+Backspace | Ctrl+Alt+Backspace |

The screenshot keys need `grim`, `slurp` and `wl-clipboard`. If pressing them does nothing, one of those is probably missing.

The program switcher works like it does elsewhere. Hold Super, press Tab until the program you want is highlighted, then let go. Shift+Tab goes backwards, and a quick tap takes you back to the program you used last.

## Touchpad

| Gesture | What it does |
|---|---|
| three fingers up | all windows side by side |
| four fingers together | all programs |
| four fingers apart | show the desktop |
| three fingers left or right | the next desktop |

On `sea-lion`, tapping doesn't click and scrolling is "natural". You can change both in Settings, Trackpad. If a gesture fires too easily, or not at all, tweak `swipe_distance` and `pinch_change` under `[gestures]` in the config.

## Windows

- Drag a window by its title bar to the left or right edge of the screen and it fills that half. Drag it into a corner for a quarter, or to the top for the whole screen. Drag it away again and it goes back to its old size.
- Super with the arrow keys works step by step: Left puts a window on the left half, then Up moves it to the top-left quarter.
- When two tiled windows sit side by side, they share the edge between them. Drag that edge and one grows while the other shrinks.
- Hold Super and drag anywhere on a window to move it. Use the right mouse button instead to resize it.
- Double-click a title bar to make the window fill the screen. Double-click again to go back.

You can change all of this in Settings, Windows, or under `[window]` in the config.

## Several desktops

Open the window overview. Your desktops are along the top. To add one, move the pointer to the top right corner and click the tile with the plus. Click a desktop to go there.

To move a window to another desktop, drag its picture onto that desktop. You can also drag the window itself to the side of the screen and hold it there for a moment.

To remove a desktop, click its round button. Its windows move to the desktop before it.

## The desktop and the dock

- Whatever's in your Desktop folder shows up on the desktop. Put the icons wherever you like. "Clean Up" in the right-click menu lines them up on the grid again.
- Drag a tile along the dock to move it, or pull it up and out to remove it. To keep a file or folder in the dock, drag it there from the file manager.
- Drag files onto the Trash in the dock to throw them away. Right-click the Trash to empty it.
- The dock can hide until you move the pointer to the bottom edge of the screen. Turn that on in Settings, Dock.
- A minimised window waits at the end of the dock as a small picture of itself. Click it to bring the window back.

## The Sea Lion look

Sea Lion brings back the desktop of OS X 10.9 Mavericks: the window frames, panel and dock, menus, icons, sounds and desktop pictures, in light and dark. It doesn't come with Mavernix. Instead, the **Theme Installer** builds it on your computer from an OS X installer you already have. The result contains Apple's artwork, so please keep it to yourself and don't share it.

These installers work:

| OS X | Build | What to give the Theme Installer |
|---|---|---|
| 10.9.5 | 13F34 | `Install OS X Mavericks.app`, the `InstallESD.dmg` inside it, or a disk image (`.dmg`, `.iso`) made from either |
| 10.9 to 10.9.4 | | These should work too. The artwork files Sea Lion uses are the same as in 10.9.5, at least in 10.9 (13A603). Nobody has tried Apple's own installers for these versions yet. |

Modified installers, such as the ones from hackintosh distributions, won't be accepted.

1. Open the Theme Installer. After your first login its icon is on the desktop. You can delete that icon if you like, since the Theme Installer is in the app grid too.
2. Drop the installer onto its icon or into its window, or pick it there. If the installer's artwork files don't match 10.9.5's, it's turned away. If only some pictures or sounds are different, the theme is built with them as they are, and the window tells you how many.
3. Wait a few minutes. While it runs, the import needs about 4.5 GB of free space. The window shows each step and how far along it is.
4. When it's done, click "Use Sea Lion" to switch to the Sea Lion desktop. Later on, you can switch back and forth in Settings, Theme.

The import needs a few tools. If one is missing, the Theme Installer tells you which: `7zip`, `cpio`, `bzip2`, `libicns`, `ffmpeg`, `poppler` and `python-pillow`. Two more are worth having. With `bubblewrap` installed, it reads the installer in a sandbox with no network access. With `lbzip2`, it unpacks on all your processor cores, which is several times faster.

The themes are saved in `~/.local/share/mavernix/themes/`. Importing again replaces them.

## Settings

The Settings window opens on a page with an icon for every pane. To jump straight to one, type its name into the search field.

You can set up the dock, the desktop picture and which desktop you use, your displays, sound, Bluetooth, the mouse and trackpad, keyboard layouts and shortcuts, default programs, energy saving, windows, several desktops, and each program's icon. Changes apply right away.

Anything that isn't in Settings is in the desktop's config. See [configuration](configuration.md).

## If something goes wrong

- **Nothing reacts:** press Ctrl+Alt+Backspace to end the session.
- **The screen is locked and won't take your password:** press Ctrl+Alt+F3 to get a text console and log in there. Then either run `loginctl unlock-session` with the session's number (`loginctl` lists it), or run `pkill wayfire` to end the desktop.
- **The panel or the dock is gone:** give it a few seconds, they restart on their own. If they keep failing, a notice tells you.
- **The desktop won't start at all:** a notice tells you why, and a key takes you back to the login screen.
- **Logs:** `$XDG_RUNTIME_DIR/mavernix/session.log` has the compositor's log. `login.log` next to it has the shell and everything started at login. Please include the end of both when you report a bug.
