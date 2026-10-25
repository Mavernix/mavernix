# Desktop

What is behind every window: one fill.

```toml
[desktop]
background = { gradient = ["#5b6f82", "#34424f"] }
# or a wallpaper:
# background = { image = "images/desktop.jpg", fit = "cover" }
# Optional: a folder in the theme with pictures the settings window offers
# for the desktop, beside any file of the user's own.
pictures = "wallpapers"
```

Which picture the user chose is not the theme's business: it is kept with the user's other choices (`[desktop] picture` in the config). Without a choice the desktop shows `background`.

`fit = "cover"` scales a picture until it covers the whole area without distorting it, cutting off the overhang evenly. Without it a picture is stretched to the area's shape, which is right for a strip or a frame but not for a photograph.

## Files on the desktop

The files of the user's Desktop folder as icons (`src/shell/DesktopIcons.qml`; whether they are shown is the config's, `[desktop_files] show`). Without the table none are shown.

```toml
[fonts]
desktop_icon = { family = "DejaVu Sans", size = 12 }   # the names under the icons

[desktop.icons]
icon_size = 48
# Icons stand in columns, the first at the screen's far side: one icon
# begins row_pitch under the one above it, a column column_pitch beside
# the one before; the first icon begins `top` under the panel, the first
# column's middle is edge_to_icon_middle from the screen's edge, and
# bottom_room is kept free at the bottom (for a dock).
row_pitch = 92
column_pitch = 100
top = 12
edge_to_icon_middle = 58
bottom_room = 90
picked = { color = "#ffffff30", radius = 4 }    # any fill: the box around a picked file's icon
picked_reach = 4                                # how far it reaches beyond the icon
# The name under the icon: its colour, optionally a dark edge under the
# letters, the height of its line, how far under the icon it begins, how
# wide it may get before it is cut short, and the plate behind a picked
# file's name (any fill, `padding` wider than the name at each end).
name = { color = "#ffffff", shadow_color = "#000000c0", height = 18, padding = 5, gap = 4, most_width = 96, picked_background = { color = "#3a6ea5", radius = 4 } }
# Optional: a frame for a picture shown as its own icon: an edge of one
# colour and width around it, a soft shadow behind, and the share of the
# icon's square the framed picture may take. Without it the bare picture
# fills the square.
preview_frame = { color = "#ffffff", width = 3, share = 0.86, shadow_color = "#00000080", shadow_blur = 4, shadow_drop = 1 }
# The band dragged over the bare desktop to pick files.
band = { color = "#3a6ea540", border_color = "#3a6ea5", border_width = 1 }
```
