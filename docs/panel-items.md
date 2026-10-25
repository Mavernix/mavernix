# Panel items

The small icons at the panel's end: sound, network, battery, other programs' icons. Each is one short QML file in `src/shell/status/`, and the desktop config says which are shown and in what order:

```toml
[panel]
right = ["tray", "bluetooth", "volume", "network", "battery", "clock"]
```

| Name | Shows | Needs |
|---|---|---|
| `tray` | the icons other programs put on the panel | nothing; programs announce their icons themselves |
| `bluetooth` | Bluetooth on or off; the menu switches it, lists the paired devices (each with a submenu that says its charge and connects or disconnects it) and, while it is open, looks for devices nearby and pairs the one chosen | BlueZ |
| `volume` | loudness of the sound output; the wheel changes it; the menu has a slider for it, the output and input devices to choose from, Mute and the sound settings | PipeWire |
| `network` | a cable, or the wireless signal's strength; the menu names the connection, or is the whole menu of a network program that has an icon of its own on the panel (`tray_program` under `[status.network]`, by default NetworkManager's applet), whose icon the tray then leaves out | NetworkManager |
| `battery` | the charge, optionally with the percentage as text; the menu says how long it lasts and what powers the machine | UPower |
| `notifications` | opens and closes the list of past notifications | nothing |
| `search` | opens and closes the search for applications and files | nothing |

The clock is not a status item but has a menu too: the date, in the format `clock_menu_date_format` under `[panel]`.

An item whose service is not running, or that has nothing to show (no battery), takes no room.

## Adding one without writing a file

For something a command can print, the config is enough. Give a name of your own a command and list the name:

```toml
[panel]
right = ["load", "clock"]

[status.load]
command = ["sh", "-c", "cut -d' ' -f1 /proc/loadavg"]
every_seconds = 10                       # optional: run again this often; without it, once
icon = "utilities-system-monitor"        # optional: an icon after the text
click_command = ["foot", "-e", "top"]    # optional: what pressing it starts
```

The panel shows the first line the command prints, in the panel's own type and colour. (`src/shell/StatusFromConfig.qml` is the component behind it; it can also give the item a menu of commands.)

## Adding one as a file

1. Write `NAME.qml` in a folder of your own that the config lists:

   ```toml
   [panel]
   item_folders = ["/home/me/.config/mavernix/status"]   # a full path
   ```

   Folders in the config are searched first, so a file there with the name of a built-in item replaces it.

2. Put `"NAME"` into `left` or `right` in the config.

The file is a `StatusItem` (`src/shell/StatusItem.qml`). The smallest one:

```qml
import QtQuick
import qs            // the shell's own components

StatusItem {
    iconName: "network-wired"
}
```

What an item can set:

| Property | Meaning |
|---|---|
| `iconName` | the icon, named as Linux desktops name icons, without "-symbolic": `audio-volume-high`, `network-wired`, `weather-clear` |
| `text` | a short text before the icon (or alone, with no `iconName`) |
| `shown` | `false` while there is nothing to show |
| `menuItems` | a menu to open when pressed; each entry is `{ id, label, enabled, separator, checkable, checked, shortcut: [], children: [] }`. An entry with `slider: VALUE` (0 to 1, and `enabled: false`) is drawn as a slider; do not change the items while it is dragged (see `src/shell/status/volume.qml`) |

What it can react to: `onChosen: id => …` (a menu entry was picked), `onSlid: (id, value) => …` (a slider in the menu was moved), `onOpening` (its menu is about to open; `open` says whether it is), `onActivated` (pressed, and it has no menu), `onScrolled: notches => …` (the wheel).

The item is handed `panel` (the panel window: `panel.settings.config` is the desktop config, so an item can have settings of its own under `[status.NAME]`) and `itemRow`.

## How an item gets its look

An item never names a picture file. It names an icon, and the theme decides:

1. If the theme lists a picture for that name in `[panel.status.icons]`, that picture is used.
2. Otherwise the icon theme's one-colour icon of that name is used.

Either way only the icon's shape is taken; it is painted in the panel's text colour, and in the highlighted text colour while its menu is open. So a new item fits every theme without the themes knowing about it. (`tray` is the exception: other programs' icons are shown in their own colours.)

Spacing and size come from `[panel.status]` in the theme ([theme format: panel](themes/panel.md)).
