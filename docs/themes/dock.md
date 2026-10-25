# Dock

A row of application icons standing on a shelf at the bottom of the screen. Which applications, and whether icons grow under the pointer, is behaviour and lives in the config.

Several sizes follow the tile size. Such a value is written as a pair `[factor, offset]` and means *factor × tile size + offset*.

```toml
[dock]
icon_size = 64            # width and height of an icon (the config may override)
compact_icon_size = 52    # optional: the size on low screens...
compact_up_to_height = 800  # ...up to this screen height

tile_margin = 1           # a tile is the icon plus this on each side; tiles touch
base = [0.2, 3]           # from the screen's edge up to the line the tiles stand on
headroom = 6              # kept free above the tiles
end_padding = [0.475, 0]  # how far the shelf reaches beyond the row, each side
row_shift = 0             # moves the row sideways from the centred position
end_trim = 0              # shortens the shelf's right end

# When the row is too long for the screen, tiles shrink: if
# tile × (tiles + fit_spare_tiles) is more than screen width + fit_overhang.
fit_spare_tiles = 2
fit_overhang = 12

# Magnification (if the config turns it on).
largest_icon_size = 128         # the icon under the pointer; also the largest size the dock can be dragged to
smallest_icon_size = 16         # optional: the smallest size it can be dragged to (dragging a separator up or down resizes the dock)
magnification_reach = 3         # how many tiles to each side are affected
magnification_ramp_ms = [1, 42] # switching on and off takes a + b × ln(1 + change / 2)

pressed_darken = 0.4      # a pressed icon's colours are multiplied by this
tile_slide_ms = 270       # tiles and the shelf's ends slide for this long when the row changes
tile_appear_ms = 350      # a new tile grows in and fades in for this long...
tile_appear_scale = 0.1   # ...starting from this share of its size

[dock.shelf]
shape = "box"             # or "slab", see below
height_above_base = [1.0, -4]  # the shelf's height is the base plus this
```

**A box shelf** is one fill:

```toml
[dock.shelf]
shape = "box"
height_above_base = [1.0, -4]
background = { color = "#2b3138e6", radius = 10 }
```

**On the shelf.** All of these tables except `indicator`, `separator` and `launch_bounce` are optional.

```toml
[dock.indicator]          # the light under a running application
fill = { color = "#ffffff", radius = 1 }   # any fill; a picture is not stretched if width and height are its size
width = 16
height = 3
center_height = 4         # its middle, above the screen's edge
# Optional smaller light for small tiles:
small_fill = { image = "images/dock-indicator-small.png" }
small_width = 28
small_below_tile_size = 28

[dock.separator]          # stands between two groups of icons
width = 0.5               # its place in the row, in tiles
bottom = 8                # its box: from this far above the screen's edge...
top_gap = 5               # ...to this far below the shelf's top
image = "images/dock-separator.png"   # a picture fitted into the box, or:
color = "#ffffff40"       # an upright line
line_width = 1

[dock.reflection]         # each icon mirrored in the shelf
opacity = 0.315
overlap = 5               # the reflection starts this far above the icon's lower edge
clip_bottom = 3           # nothing is mirrored into the lowest rows
blur = 0.047              # radius, as a share of the icon's size
contrast = 1.45           # 1 = unchanged
brightness = 0.2          # added; 0 = unchanged

[dock.floor_shadow]       # the icon's shape, flattened, on the shelf
color = "#00000040"
height = 0.5              # as a share of the icon's height
blur = 0.047

[dock.icon_shadow]        # the icon's shape behind the icon
color = "#00000047"
blur = 7                  # radius in pixels
raise = 1                 # moved up by this

[dock.trash]              # optional: pictures for the trash tile (the config decides whether there is one)
empty_image = "images/trash-empty.svg"
full_image = "images/trash-full.svg"
# Without pictures, the icons named here are taken from the system's icon
# theme (defaults: "user-trash" and "user-trash-full").
empty_icon = "user-trash"
full_icon = "user-trash-full"

[dock.label]              # optional: the name above the tile under the pointer; font: [fonts].dock_label
background = "#1e1e1e99"  # a bubble with fully round ends
text_color = "#ffffff"
height = 20
padding = 11              # space beside the text
pointer_width = 12        # a triangle under the bubble's middle; 0 for none
pointer_height = 6
gap = 28                  # from the tile's top up to the pointer's tip
fade_out_ms = 167         # it appears at once and fades away

[dock.launch_bounce]      # while an application starts, its icon is thrown up and falls back
half_period_ms = 350      # time up, and time down
height = [0.2, 4.4]       # of the tile's size
longest_ms = 120000       # give up after this
```

## The menus on the dock's tiles

A secondary press on a tile or a separator opens a menu above it. It looks like any other menu unless the theme says otherwise: `[dock.menu]` holds whatever is to differ from `[menu]`, key for key, and two things of its own.

```toml
[dock.menu]                    # any key of [menu], and:
background = "#202020f0"
text_color = "#ffffff"
pointer = { width = 24, height = 12 }   # optional: a triangle from the menu's bottom edge towards the tile, in the menu's background and rim
tile_gap = 4                   # between the menu (or its pointer's tip) and the tile's icon
```

## Minimised windows

A minimised window has a tile of its own in the dock's last group. A theme chooses what the tile shows:

```toml
[dock.minimized]
shows = "picture"   # "picture": a small picture of the window; "icon" (the default): its application's icon
badge_share = 0.4   # with a picture: the application's icon in the tile's lower far corner, this share of the tile; 0 for none
```

The picture fills the tile in the window's longer direction and stands in the middle of the other. It is taken when the minimise effect (`[animation.minimize]`) is over; the tile is empty until then.

## Removing a tile

A tile dragged out of the dock and let go is removed. A theme may give pictures to show where it was dropped; without the table the tile simply goes.

```toml
[dock.remove_effect]
image = "images/remove.png"   # the pictures, one below the other in one file
frames = 5                    # how many
size = 64                     # how large one is drawn
frame_ms = 50                 # how long each stays
```

## Folders in the dock

A place in the dock may be a folder (the config's doing); a press shows its contents: as a list, which is a menu like those above; as a grid of icons in a box that points at the tile; or as a fan, the files one above the other rising from the tile, each with its name beside it. The names' font is `[fonts].dock_folder`. How the grid comes and goes is `[animation.dock_folder]` (`kind` "grow", which enlarges it out of its tile as it fades in, "fade" or "none"; `duration_ms`, optionally `close_duration_ms`); for the fan it is `[animation.dock_folder_fan]` (`kind` "rise", which lets the entries climb out of the tile along the fan's line, or "none").

```toml
[dock.folder]
open_image = "images/folder-open-tile.png"   # optional: the tile's picture while its folder is open

[dock.folder.fan]
icon_size = 40
pitch = 46                # from one entry to the next
turn_per_entry = 0        # degrees each entry is turned further than the one below: the fan's line is then an arc; 0 is a straight column
most_entries = 8          # the fan shows so many at most, counting the last one, which opens the folder and says how many files are not shown
label_gap = 6             # between a name's bubble and its icon
label_lift = 0            # optional: how far the bubble's middle stands above the icon's
label_width = 220         # a longer name loses its middle
label = { background = "#202020f0", text_color = "#ffffff", height = 22, padding = 8 }   # optional: the bubbles' looks, if not those of [dock.label]
open_image = "images/open-folder.png"   # optional, as in the grid below
open_image_size = 56                   # optional, likewise

[dock.folder.grid]
side_padding = 8          # between the box's sides and the first and last column
cell_width = 96           # from one entry to the next
cell_height = 92
first_row_top = 30        # from the box's top edge to the first row's icons
bottom_padding = 6        # below the last row
icon_size = 48
label_baseline = 66       # the row below an icon's top on which its name stands
label_width = 88          # a longer name loses its middle
label_color = "#202020"
label_shadow_color = "#000000a0"   # optional
title_baseline = 19       # the folder's name, centred at the top
title_color = "#20202070"
screen_margin = 8         # the box keeps this far from the screen's sides
open_image = "images/open-folder.png"   # optional: the picture of the last entry, which opens the folder itself (else the icon theme's "folder-open")
open_image_size = 56                   # optional: its size, if not that of an icon
```

Without more, the grid's box is drawn like the dock's menus. A theme with pictures for it adds:

```toml
[dock.folder.grid.box]
pieces = "images/folder-box"   # the files are folder-box-tl.png, -tm, -tr, -l, -c, -r, -bl, -bm, -br (corners, edges that stretch, middle) and -bt, a stretch of the bottom edge with the pointer on it
piece_size = 40                # width and height of a corner
margins = [27, 22, 27, 31]     # how far the pictures reach beyond the box (rim, shadow): left, top, right, bottom
pointer_width = 45             # width of the -bt picture
pointer_height = 13            # how far its pointer reaches below the box
```

## The slab shelf

**A slab shelf** is a flat board seen from above and in front: a top surface the icons stand on, a front face below it, and a bright line where the two meet. Its ends lean outwards towards the viewer, so the front edge is wider than the back edge. The row of icons is as wide as the back edge.

```toml
[dock.shelf]
shape = "slab"
height_above_base = [0.5, 3]

slant = 0.6               # how far each end moves outwards per pixel downwards
face_height = 3           # height of the front face
line_height = 1           # height of the bright line on the surface's front edge
back_corner_radius = 3    # rounding where an end meets the back edge

# The front corners are rounded too, and their size grows with the shelf:
# the size at two shelf heights; in between and beyond it changes evenly.
front_corner_heights = [32, 128]
front_corner_sizes = [4, 12]

clear_above = 1           # rows left empty above the back edge
clear_beside = 1          # columns left empty beside the front corners

# Colours, each list running from the front of the shelf to the back, over
# the shelf's full height. The face only shows the first few rows of its list.
surface = ["#ffffffba", "#d7d7d7a6", "#aaaaaa8f"]
face = ["#9c9c9ccc", "#d2d2d2db"]
line = "#ffffffa6"

# Optional thin dark rim just outside the slab.
outline = "#0000001f"
outline_width = 1
```

Translucent colours let the desktop show through the shelf.

*Not supported yet:* a dock at the side of the screen, labels under the icons.
