# Panel items

Panel items are the small icons at the end of the panel: sound, network, battery, and other programs' icons. Each one is a short QML file in `src/shell/status/`. The desktop config says which ones are shown, and in what order:

```toml
[panel]
right = ["tray", "bluetooth", "volume", "network", "battery", "clock"]
```

| Name | Shows | Needs |
|---|---|---|
| `tray` | the icons other programs put on the panel | nothing; programs announce their icons themselves |
| `bluetooth` | whether Bluetooth is on or off. The menu switches it, and lists the paired devices, each with a submenu that shows its charge and connects or disconnects it. While the menu is open, it looks for devices nearby and pairs the one you choose | BlueZ |
| `volume` | how loud the sound output is. The scroll wheel changes it. The menu has a slider for it, the output and input devices to choose from, Mute, and the sound settings | PipeWire |
| `network` | a cable, or the strength of the wireless signal. The menu turns Wi-Fi on and off and lists the networks in reach, with a lock on those that need a password and their signal strength; choosing one joins it, asking for the password if needed. A network program's tray icon (`tray_program` under `[status.network]`, NetworkManager's applet by default) is left out; with `menu = "program"` the menu is that program's whole menu instead | NetworkManager |
| `battery` | the charge, optionally with the percentage as text. The menu says how long it will last and what's powering the machine | UPower |
| `notifications` | opens and closes the list of past notifications | nothing |
| `search` | opens and closes the search for applications and files | nothing |

The clock isn't a status item, but it has a menu too. It shows the date, in the format set by `clock_menu_date_format` under `[panel]`.

If an item's service isn't running, or it has nothing to show (say, there's no battery), it takes up no room.

## Adding one without writing a file

If a command can print what you want to show, the config is all you need. Give a name of your own a command, and list that name:

```toml
[panel]
right = ["load", "clock"]

[status.load]
command = ["sh", "-c", "cut -d' ' -f1 /proc/loadavg"]
every_seconds = 10                       # optional: how often to run it again; without it, it runs once
icon = "utilities-system-monitor"        # optional: an icon after the text
click_command = ["foot", "-e", "top"]    # optional: what clicking it starts
```

The panel shows the first line the command prints, in the panel's own font and colour. (The component behind this is `src/shell/StatusFromConfig.qml`. It can also give the item a menu of commands.)

## Adding one as a file

1. Write `NAME.qml` in a folder of your own, and list that folder in the config:

   ```toml
   [panel]
   item_folders = ["/home/me/.config/mavernix/status"]   # a full path
   ```

   Folders from the config are searched first, so a file there with the same name as a built-in item replaces it.

2. Put `"NAME"` into `left` or `right` in the config.

The file is a `StatusItem` (`src/shell/StatusItem.qml`). Here's the smallest one:

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
| `iconName` | the icon, named the way Linux desktops name icons, without "-symbolic": `audio-volume-high`, `network-wired`, `weather-clear` |
| `text` | a short text before the icon (or on its own, with no `iconName`) |
| `shown` | `false` while there's nothing to show |
| `menuItems` | a menu that opens when it's clicked. Each entry is `{ id, label, enabled, separator, checkable, checked, shortcut: [], children: [] }`. An entry with `slider: VALUE` (0 to 1, and `enabled: false`) is drawn as a slider. Don't change the items while the slider is being dragged (see `src/shell/status/volume.qml`) |

What it can react to:

- `onChosen: id => …`: a menu entry was picked.
- `onSlid: (id, value) => …`: a slider in the menu was moved.
- `onOpening`: its menu is about to open. (`open` tells you whether the menu is open.)
- `onActivated`: it was clicked and has no menu.
- `onScrolled: notches => …`: the scroll wheel moved.

The item is handed `panel` and `itemRow`. `panel` is the panel window, and `panel.settings.config` is the desktop config, so an item can have settings of its own under `[status.NAME]`.

## How an item gets its look

An item never names a picture file. It names an icon, and the theme decides what that looks like:

1. If the theme lists a picture for that name in `[panel.status.icons]`, that picture is used.
2. Otherwise, the icon theme's one-colour icon with that name is used.

Either way, only the icon's shape is used. It's painted in the panel's text colour, and in the highlighted text colour while its menu is open. That way a new item fits every theme without the themes having to know about it. (`tray` is the exception: other programs' icons keep their own colours.)

Spacing and size come from `[panel.status]` in the theme ([theme format: panel](themes/panel.md)).
