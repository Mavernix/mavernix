# Publishing an app in the App Store

The App Store (`mavernix-app-store`) offers apps made for Mavernix. It finds them on GitHub: every repository of the Mavernix organisation with the topic `mavernix-app` is an app. What the store shows and installs comes from the repository's latest release. So publishing an app, or an update to it, is making a release. [Bandstand](https://github.com/Mavernix/bandstand) is a working example.

## What a release needs

Three kinds of files, attached to the release:

| File | What it is |
|---|---|
| `mavernix-app.toml` | the listing: what the store shows (below) |
| the files the listing names | its icon and screenshots |
| `NAME-x86_64.tar.gz` | the bundle: everything the app needs to run, in one folder called `NAME/`, where NAME is the repository's name |

The release's tag is the version (`v1.2.0` shows as 1.2.0). Its notes are shown under "What's New", and can use Markdown.

## The listing

```toml
title = "Bandstand"
summary = "A music player for YouTube Music"   # one line, under the name
category = "Music"                             # optional
developer = "Mavernix"                         # optional; the organisation if left out
license = "GPL-3.0-or-later"                   # optional
description = """
Optional. A few paragraphs for the app's page, in Markdown.
"""

# Files attached to the release.
icon = "bandstand.svg"                         # SVG or PNG, square
screenshots = ["screenshot.png"]               # optional

# Inside the bundle.
command = "bin/bandstand"                      # the program to start
desktop_entry = "share/applications/bandstand.desktop"
```

## The bundle

The store unpacks the bundle into `~/.local/share/mavernix/apps/NAME/bundle/`. It copies the desktop entry to `~/.local/share/applications/`, pointing its `Exec` at `command` and its `Icon` at the listing's icon. That puts the app in the app grid. Name the entry after the app's window: the dock recognises windows by it (`QGuiApplication::setDesktopFileName` in a Qt app).

A bundle has to run wherever it lands, so:

- **The program finds its files next to itself**, not at a fixed path: QML, helpers and the like relative to where the program is (`QCoreApplication::applicationDirPath()`).
- **It brings what Mavernix doesn't.** Mavernix already needs Qt 6 (Quick, Multimedia, SVG, Wayland) and Python 3, so those are there. Anything else goes into the bundle. Python packages go in as pure-Python wheels, so they work with any Python 3.
- **It's built on the oldest system Mavernix supports**, Fedora 44 today. A program built against an older glibc and Qt also runs on newer ones (Fedora 45, Arch); the other way round it doesn't.

Bandstand's `packaging/make-bundle` does all of this in a Fedora 44 container, and its `.github/workflows/release.yml` runs it and makes the release when a version tag is pushed.

## What the store checks

- The bundle's checksum. GitHub gives one for every file attached to a release, and the store won't install a bundle that doesn't match it.
- That the bundle is one folder named after the app, with nothing in it that would land outside that folder, no device files, and no set-user-ID programs.
- That `command` and `desktop_entry` are paths inside the bundle, and are there.

A repository whose release lacks something is left out of the store, and the reason is written to the store's log, `~/.cache/mavernix/app-store.log`.

## Trying a release before it's public

`MAVERNIX_APP_STORE_API` points the store at another copy of GitHub's API. It's what `tests/test_app_store.py` does: a small local web server answers like GitHub would.

```sh
MAVERNIX_APP_STORE_API=http://127.0.0.1:8000/api mavernix-app-store
```

The server needs `api/orgs/Mavernix/repos` (a list of repositories with their `topics`) and `api/repos/Mavernix/NAME/releases/latest` (a release whose `assets` have a `name`, `size`, `browser_download_url` and `digest`), as GitHub's API returns them.
