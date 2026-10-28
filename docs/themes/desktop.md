# Desktop

The desktop is what sits behind every window. It's a single fill.

```toml
[desktop]
background = { gradient = ["#5b6f82", "#34424f"] }
# or a wallpaper:
# background = { image = "images/desktop.jpg", fit = "cover" }
# Optional: a folder in your theme with pictures that the settings window
# offers for the desktop, next to any files of the user's own.
pictures = "wallpapers"
```

Your theme doesn't decide which picture the user picks. That choice is kept with the user's other choices (`[desktop] picture` in the config). If they haven't picked one, the desktop shows `background`.

`fit = "cover"` scales a picture until it covers the whole area without distorting it, and trims the overhang evenly. Without it, the picture is stretched to the shape of the area. That's fine for a strip or a frame, but not for a photo.

## Files on the desktop

This table shows the files in the user's Desktop folder as icons (`src/shell/DesktopIcons.qml`). Whether they're shown at all is up to the config (`[desktop_files] show`). If your theme leaves the table out, no files are shown.

```toml
[fonts]
desktop_icon = { family = "DejaVu Sans", size = 12 }   # the names under the icons

[desktop.icons]
icon_size = 48
# Icons stand in columns, starting at the far side of the screen. Each icon
# starts row_pitch below the one above it, and each column sits column_pitch
# beside the one before. The first icon starts `top` below the panel. The
# middle of the first column is edge_to_icon_middle from the screen's edge.
# bottom_room is kept free at the bottom (for a dock).
row_pitch = 92
column_pitch = 100
top = 12
edge_to_icon_middle = 58
bottom_room = 90
picked = { color = "#ffffff30", radius = 4 }    # any fill: the box around a picked file's icon
picked_reach = 4                                # how far it reaches past the icon
# The name under the icon: its colour, an optional dark edge under the
# letters, its line height, how far below the icon it starts, and how wide
# it can get before it's cut short. picked_background is the plate behind a
# picked file's name (any fill, `padding` wider than the name at each end).
name = { color = "#ffffff", shadow_color = "#000000c0", height = 18, padding = 5, gap = 4, most_width = 96, picked_background = { color = "#3a6ea5", radius = 4 } }
# Optional: a frame for a picture that's shown as its own icon. It has an
# edge of one colour and width, and a soft shadow behind it. `share` is how
# much of the icon's square the framed picture may take. Without it, the
# bare picture fills the square.
preview_frame = { color = "#ffffff", width = 3, share = 0.86, shadow_color = "#00000080", shadow_blur = 4, shadow_drop = 1 }
# The band you drag over the empty desktop to pick files.
band = { color = "#3a6ea540", border_color = "#3a6ea5", border_width = 1 }
```
