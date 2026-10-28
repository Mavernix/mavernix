# Panel

The panel is the bar along one edge of the screen. The desktop config decides which edge and what it shows. Your theme decides how it looks.

```toml
[fonts]
panel = { family = "DejaVu Sans", size = 13 }

[panel]
height = 28                           # including the separator lines
background = { color = "#e8e8e8" }    # any fill
# Optional one-pixel lines along the panel's inner edge, the one facing the
# desktop. The first colour is the outermost line. The background fill
# covers the rest of the height.
separator = ["#b0b0b0"]
# Optional one-pixel lines along the panel's outer edge, the one at the
# screen's edge, drawn over the fill. The first colour is the outermost.
highlight = ["#ffffff80"]
text_color = "#202020"
app_name_bold = false                 # whether the active application's name is bold
item_spacing = 12                     # gap between neighbouring items
edge_padding = 10                     # gap between the screen edge and the first item
logo = "images/logo.png"              # optional picture for the "logo" item
logo_spacing = 8                      # optional: gap after the logo, if it should differ from item_spacing
```

If the fill's opacity is below 1 (a translucent picture, say), the desktop shows through the panel.

## Status items

These are the small icons at the end of the panel: sound, network, battery and other programs' icons (see [panel items](../panel-items.md)). Everything here is optional.

```toml
[panel.status]
spacing = 12        # gap between neighbouring status items and after the clock; item_spacing without it
                    # (an open item's highlight reaches half of it past the item on each side)
text_gap = 4        # gap between an item's text and its icon
icon_size = 16      # the size icons from the icon theme are shown at

[panel.status.gaps]             # any names; optional: a different gap than `spacing` before the item showing this icon
"preferences-system-notifications" = 9

[panel.status.icons]            # any names: your theme's own picture for an icon name
"audio-volume-high" = "images/status/volume-high.png"
"network-wireless-signal-good" = "images/status/wifi-3.png"
```

A status item asks for its icon by the name Linux desktops use for it (freedesktop.org's icon naming, without the "-symbolic" ending). If this table has a picture for that name, it's shown at its own size. If not, you get the one-colour icon of that name from the icon theme (`[icons]`).

Either way, only the picture's shape counts. It's painted in `text_color`, and in `title_highlight_text_color` while the item's menu is open. So make the pictures here one-colour drawings on a clear background. That way, a status item this table doesn't know about still fits your theme.

The names the built-in items ask for: `audio-volume-muted`, `-low`, `-medium`, `-high`; `network-wired`, `network-offline`, `network-wireless-offline`, `network-wireless-signal-none`, `-weak`, `-ok`, `-good`, `-excellent`; `bluetooth-active`, `bluetooth-disabled`; `battery-level-0` to `battery-level-100` in tens, `battery-good-charging`, `battery-full-charged`, `battery-missing`.
