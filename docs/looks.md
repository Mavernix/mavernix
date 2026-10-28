# Looks

A *look* is something the Theme Installer can make: one or more themes, plus the desktops that use them, built from an installer the user provides. Each look is a folder in `src/apps/theme-installer/backend/looks/`:

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
source = "the OS X 10.9.5 installer"             # shown as "Made from …" on the first page
drop_hint = "Install OS X Mavericks, its InstallESD.dmg, or a disk image made from them"
licence_note = "…"                                 # optional, shown under the look

themes = ["sea-lion", "sea-lion-dark"]                 # what the import makes
desktops = ["sea-lion", "sea-lion-dark"]               # configs next to look.toml; "Use" switches to the first one

recognise = ["BaseSystem.dmg", "Packages/Essentials.pkg"]   # files inside a dropped image
wrapped_in = "InstallESD.dmg"                      # or inside this file in the dropped image, one level deeper
inside_program = "Contents/SharedSupport/InstallESD.dmg"    # where the image is when a program folder is dropped
```

When you drop a file on the Theme Installer, it checks it against every look's `recognise` list. That takes a second or two, because it only needs the image's list of files.

## Checking the installer

`needed-files.txt` lists every file the look takes, with its SHA-256. The import checks each file as it comes out of the installer:

- If a file is missing, the import stops.
- Some files are read by position: the look's generators rely on picture numbers, offsets or symbols in them (see `DECODED_BY_POSITION` in `installer_image.py`). If one of those differs, the import stops too, because the same numbers would point at other pictures.
- Any other file that differs is used as it is. The last page tells you how many differed.

`python3 import_theme.py fingerprint SOURCE` prints `needed-files.txt` with the checksums from another copy of the installer. The list only holds checksums, so you can share it without sharing any of the installer's files.

## Reading the installer

7-Zip reads the whole disk image every time it opens one, even just to list it. For OS X 10.9's 5.3 GB InstallESD.dmg on a slow disk, that's minutes. So the import opens the image as few times as it can:

- The list of files it reads to recognise a dropped image is kept in `~/.cache/mavernix/listings/`, keyed by the image's path, size and change time. Only the newest four are kept. The import uses this list instead of listing the image again.
- The parts the look needs are copied out together in one pass. Then the import pulls their files out and deletes the parts one by one. This needs enough free space in `~/.cache` for all of those parts at once (4.4 GB for Sea Lion).

The window reads the dropped file alongside its other work and shows that it's doing so. If you choose another file in the meantime, that one replaces it.

## The log

Everything the Theme Installer does goes into `~/.cache/mavernix/theme-installer.log` (it honours `$XDG_CACHE_HOME`). That includes the check of the computer, each file it reads and what it makes from it, every stage of an import (including the part in the sandbox), the error output of the back end and its tools, and switching to the new desktop. When something fails, the window tells you where the log is.

## Not yet

The generators that turn the files into themes belong to Sea Lion alone (`make_theme.py` and the rest, run by `import_theme.py`). A second look will bring its own, and whatever the looks end up sharing will move into a common part.
