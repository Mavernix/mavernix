# Looks

A *look* is what the Theme Installer can make: one or more themes and the desktops that use them, built from an installer the user provides. Each look is a folder in `src/apps/theme-installer/backend/looks/`:

```
looks/sea-lion/
  look.toml          what the window says about it, how its installer is recognised, what it installs
  needed-files.txt   the files taken from the installer, each with its SHA-256
  sea-lion.toml        its desktops (configs), installed into ~/.local/share/mavernix/config/
  sea-lion-dark.toml
```

## look.toml

```toml
name = "Sea Lion"
description = "Window frames, panel and dock, … in a classic look, light and dark."
source = "the OS X 10.9.5 installer"             # "Made from …" on the first page
drop_hint = "Install OS X Mavericks, its InstallESD.dmg, or a disk image made from them"
licence_note = "…"                                 # optional, shown under the look

themes = ["sea-lion", "sea-lion-dark"]                 # what the import makes
desktops = ["sea-lion", "sea-lion-dark"]               # configs beside look.toml; the first is the one "Use" switches to

recognise = ["BaseSystem.dmg", "Packages/Essentials.pkg"]   # files inside a dropped image
wrapped_in = "InstallESD.dmg"                      # or this file inside it, opened one level deeper
inside_program = "Contents/SharedSupport/InstallESD.dmg"    # where the image is when a program folder is dropped
```

A file dropped on the Theme Installer is matched against every look's `recognise` within a second or two, from the image's list of files alone.

## Checking the installer

`needed-files.txt` lists every file the look takes, with its SHA-256. The import checks each file as it comes out of the installer:

- a file that is missing ends the import;
- a file the look's generators read by position (picture numbers, offsets, symbols; `DECODED_BY_POSITION` in `installer_image.py`) that differs ends it too: the same numbers would name other pictures;
- any other file that differs is used as it is, and the last page says how many differ.

`python3 import_theme.py fingerprint SOURCE` prints `needed-files.txt` with the checksums of another copy of the installer. The list holds only checksums, so it can be shared without any of the installer's files.

## Reading the installer

7-Zip reads the whole of a disk image each time it opens one, even only to list it: for OS X 10.9's 5.3 GB InstallESD.dmg, minutes from a slow disk. So the import opens the image as few times as it can:

- the list of files read to recognise a dropped image is kept in `~/.cache/mavernix/listings/` (by the image's path, size and time of change; the newest four), and the import uses it instead of listing the image again;
- the parts the look needs are copied out together, in one pass, then mined for their files and deleted one by one. This needs their size together free in `~/.cache` (4.4 GB for Sea Lion).

The window reads the dropped file beside its own work and shows that it is doing so; a file chosen meanwhile replaces it.

## The log

Everything the Theme Installer does goes to `~/.cache/mavernix/theme-installer.log` (`$XDG_CACHE_HOME` is honoured): the check of the computer, each file read and what it makes, every stage of an import (the part in the sandbox too), the error output of the back end and its tools, and switching to the new desktop. The window names the log when something fails.

## Not yet

The generators that turn the files into themes are Sea Lion's own (`make_theme.py` and the rest, run by `import_theme.py`). A second look brings its own; what the looks then share moves into a common part.
