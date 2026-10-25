# Looks

A *look* is what the Theme Installer can make: one or more themes and the desktops that use them, built from an installer the user owns. Each look is a folder in `src/apps/theme-installer/backend/looks/`:

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
source = "Apple's own OS X 10.9.5 installer"       # "Made from …" on the first page
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

## Not yet

The generators that turn the files into themes are Sea Lion's own (`make_theme.py` and the rest, run by `import_theme.py`). A second look brings its own; what the looks then share moves into a common part.
