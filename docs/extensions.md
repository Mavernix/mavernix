# Extensions

Extensions add things to the desktop that aren't part of it: features only some people want. Each one is a folder in `~/.config/mavernix/extensions/`, or in `/usr/share/mavernix/extensions/` if it's installed for everyone. If you have a folder with the same name as an installed one, yours takes its place. The desktop config decides which extensions are switched on (`[extensions] enabled`). You can also change that while the desktop runs, in the Extensions pane of the settings window.

An extension is QML plus whatever small programs it brings. Nothing gets compiled. The shell reads its files when you switch it on, so after you change something, switch it off and on again to see it.

## What a folder holds

```
NAME/
  extension.toml       what it is and what it brings (below)
  Background.qml       optional: an object that keeps running while it is switched on
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
                                  # without one, the theme's plug-in icon is used

background = "Background.qml"     # optional

[panel_items]                     # optional: items it wants on the panel, by file name in status/
right = ["clipboard"]             # shown before the desktop's own items at that end;
left = []                         # if the desktop config lists an item itself, it stays where the config puts it

[settings]                        # optional: its settings and their defaults
keep_items = 20
show_counter = true

[settings_labels]                 # optional: yes-or-no settings that get a tick box in the settings window
show_counter = "Show how many items are kept"

[words]                           # optional: its wording, so you can change it in one place
clear = "Clear History"
```

You can change a setting in the settings window (only the yes-or-no ones that have a label), or in the desktop config, in a table named after the extension: `[extensions.NAME]`. Changes reach the extension while it runs. Both `extension.settings` in its running part and `panel.extensions.info("NAME").settings` in a panel item follow along.

## The rules the desktop's own parts follow hold here too

- **Looks are not in the code.** Sizes, colours and timings go into `looks/`: one file per theme the extension knows, and `default.toml` for the rest. Under the `plain` theme, nothing from another theme's look may show.
- **Reuse.** Use what the shell already has (`import qs`: `StatusItem`, `Fill`, `MenuBox`, `ThemeValues`) before drawing your own.

## The running part (`background`)

This is a QML file whose top object is a `Scope` (or any object). The shell creates it when the extension is switched on and destroys it when it's switched off. It hands it these:

| Property | What it is |
|---|---|
| `extension` | what the session found: `directory`, `manifest` (the extension.toml), `looks` (the looks file for the theme in use), `settings` (its `[settings]` with the user's laid over) |
| `settings` | the desktop's settings: `settings.theme`, `settings.config` |
| `shell` | the shell's root; `shell.run("app_grid")` and the like |
| `screen` | the screen to show things on |
| `extensions` | the shell's `Extensions` object |

Declare the ones you use (`required property var extension`). If it starts something, like a helper program, it has to end it itself when it's destroyed.

## A panel item

A panel item is a file in `status/` built from `StatusItem`, just like the desktop's own items. [Panel items](panel-items.md) lists what an item can set. It reaches its extension through the panel:

```qml
readonly property var extension: panel.extensions.info("NAME")      // directory, manifest, looks, settings
readonly property var running: panel.extensions.running("NAME")     // the running part, or null
```

An item can show its own picture instead of one from an icon theme: `pictureSource: "file://" + extension.directory + "/images/mine.svg"` and `pictureHeight: statusLooks.icon_size`. It's painted in the panel's colour, like every other item.

## Trying one

Switch it on in the desktop config (`[extensions] enabled = ["NAME"]`) or in the settings window. Then check the shell's log (`$XDG_RUNTIME_DIR/mavernix/login.log`) for "extensions:" and for QML errors. If an extension has an error in a file, it's left out with a message and the desktop carries on. `dev/run-in-container` is a safe place to try one.
