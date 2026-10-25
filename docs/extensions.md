# Extensions

Things added to the desktop that are not part of it: features only some people want. Each is a folder in `~/.config/mavernix/extensions/`, or in `/usr/share/mavernix/extensions/` for extensions installed for everyone (a folder of yours with the same name takes the place of an installed one). Which are switched on is the desktop config's (`[extensions] enabled`), and can be changed while the desktop runs, in the settings window's Extensions pane.

An extension is QML and whatever small programs it brings; nothing is compiled. The shell reads its files when it is switched on, so a change shows after switching it off and on.

## What a folder holds

```
NAME/
  extension.toml       what it is and what it brings (below)
  Background.qml       optional: an object kept running while it is switched on
  status/ITEM.qml      optional: items for the panel
  looks/THEME.toml     optional: how it looks under a theme; looks/default.toml for every other theme
  …                    anything else it needs: pictures, helper programs
```

`extension.toml`:

```toml
title = "Clipboard history"       # its name in the settings window
description = "One sentence saying what it adds."
version = "0.1.0"
icon = "images/icon.png"          # optional: its picture in the settings window, a file in its folder;
                                  # without one the theme's plug-in icon stands in

background = "Background.qml"     # optional

[panel_items]                     # optional: items it wants on the panel, by file name in status/
right = ["clipboard"]             # shown before the desktop's own items at that end;
left = []                         # an item the desktop config lists itself stays where the config has it

[settings]                        # optional: its settings and their defaults
keep_items = 20
show_counter = true

[settings_labels]                 # optional: yes-or-no settings the settings window offers a tick box for
show_counter = "Show how many items are kept"

[words]                           # optional: its wording, so that it can be changed in one place
clear = "Clear History"
```

A setting is changed in the settings window (the yes-or-no ones that have a label), or in the desktop config, in a table of the extension's name: `[extensions.NAME]`. A change reaches the extension while it runs: `extension.settings` in its running part, `panel.extensions.info("NAME").settings` in a panel item, both follow.

## The rules the desktop's own parts follow hold here too

- **Looks are not in the code.** Sizes, colours and timings go into `looks/`, one file per theme the extension knows, and `default.toml` for the rest. Under the `plain` theme nothing of another theme's look may show.
- **Reuse.** Take what the shell has (`import qs`: `StatusItem`, `Fill`, `MenuBox`, `ThemeValues`) before drawing your own.

## The running part (`background`)

A QML file whose top object is a `Scope` (or any object). The shell creates it when the extension is switched on and destroys it when it is switched off, and hands it:

| Property | What it is |
|---|---|
| `extension` | what the session found: `directory`, `manifest` (the extension.toml), `looks` (the looks file for the theme in use), `settings` (its `[settings]` with the user's laid over) |
| `settings` | the desktop's settings: `settings.theme`, `settings.config` |
| `shell` | the shell's root; `shell.run("app_grid")` and the like |
| `screen` | the screen to show things on |
| `extensions` | the shell's `Extensions` object |

Declare the ones you use (`required property var extension`). What it starts (a helper program) it must end itself when it is destroyed.

## A panel item

A file in `status/` made of `StatusItem`, as the desktop's own are ([panel items](panel-items.md) says what an item can set). It gets at its extension through the panel:

```qml
readonly property var extension: panel.extensions.info("NAME")      // directory, manifest, looks, settings
readonly property var running: panel.extensions.running("NAME")     // the running part, or null
```

An item may show a picture of its own instead of an icon theme's: `pictureSource: "file://" + extension.directory + "/images/mine.svg"` and `pictureHeight: statusLooks.icon_size`. It is painted in the panel's colour like every item's.

## Trying one

Switch it on in the desktop config (`[extensions] enabled = ["NAME"]`) or in the settings window, then look into the shell's log (`$XDG_RUNTIME_DIR/mavernix/login.log`) for "extensions:" and for QML errors. An extension whose file has an error is left out with a message; the desktop goes on. `dev/run-in-container` is a safe place to try one.
