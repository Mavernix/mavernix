# Panel

The bar along a screen edge. Which edge, and what it shows, is in the desktop config; the theme says how it looks.

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

A fill with opacity below 1, such as a translucent picture, lets the desktop show through the panel.

## Status items

The small icons at the panel's end (sound, network, battery, other programs' icons; see [panel items](../panel-items.md)). All of this is optional.

```toml
[panel.status]
spacing = 12        # gap between neighbouring status items and after the clock; without it, item_spacing
                    # (an open item's highlight reaches half of it beyond the item on each side)
text_gap = 4        # gap between an item's text and its icon
icon_size = 16      # the size at which icons from the icon theme are shown

[panel.status.gaps]             # any names; optional: another gap than `spacing` before the item showing this icon
"preferences-system-notifications" = 9

[panel.status.icons]            # any names: a picture of the theme's own for an icon name
"audio-volume-high" = "images/status/volume-high.png"
"network-wireless-signal-good" = "images/status/wifi-3.png"
```

A status item asks for an icon by the name Linux desktops use for it (freedesktop.org's icon naming, without the "-symbolic" ending). If this table has a picture for the name, that is shown, at its own size; otherwise the icon theme's (`[icons]`) one-colour icon of that name. In both cases only the picture's shape counts: it is painted in `text_color`, and in `title_highlight_text_color` while the item's menu is open. So the pictures here should be one-colour drawings on a clear background, and a status item that this table does not know still fits the theme.

The names the built-in items ask for: `audio-volume-muted`, `-low`, `-medium`, `-high`; `network-wired`, `network-offline`, `network-wireless-offline`, `network-wireless-signal-none`, `-weak`, `-ok`, `-good`, `-excellent`; `bluetooth-active`, `bluetooth-disabled`; `battery-level-0` to `battery-level-100` in tens, `battery-good-charging`, `battery-full-charged`, `battery-missing`.
