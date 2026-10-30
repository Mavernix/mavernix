# Overlays

## Program switcher

While you hold the switching key, a plate shows every program that has windows (`src/shell/ProgramSwitcher.qml`). Without the table there's no switcher.

```toml
[fonts]
program_switcher = { family = "DejaVu Sans", size = 12 }   # the marked program's name

[program_switcher]
background = { color = "#f4f4f4f0", radius = 10 }   # any fill: the plate, in the screen's middle
icon_size = 64
icon_gap = 12
padding = [18, 18, 40, 18]          # around the row of icons: top, right, bottom, left
mark = { color = "#3a6ea540", radius = 8 }          # any fill: the box around the marked program's icon
mark_reach = 6                      # how far it reaches beyond the icon
# The marked program's name, under its icon: its colour, how far under the
# icon it begins, and optionally a plate of its own (a fill, a height, the
# space beside the name).
name = { color = "#202020", gap = 10, padding = 6 }
appear_after_ms = 150               # the plate only shows once the key has been held this long
```

## Notifications

When a program sends a notification, it shows up in a banner for a few seconds, in the screen's top corner on the end side. The banner's fonts are `[fonts].banner_title` and `[fonts].banner_text`. `[animation.banner]` sets how it comes and goes.

```toml
[notifications.banner]
width = 340
top_gap = 10                   # below the panel (or the screen's top, if the panel is elsewhere)
edge_gap = 10                  # from the screen's side
gap = 8                        # between two banners
# One background for each number of text lines under the title (none, one,
# two), each any fill, and the banner's height for each.
backgrounds = [{ color = "#f4f4f4", radius = 6 }, { color = "#f4f4f4", radius = 6 }, { color = "#f4f4f4", radius = 6 }]
heights = [44, 60, 76]
background_margins = [0, 0, 0, 0]   # optional: room inside a background picture around the panel, for its shadow (top, right, bottom, left)
icon_size = 32
icon_inset = [10, 6]           # the icon's distance from the left and the top
text_left = 52
text_right = 12
title_baseline_row = 20        # the rows of the panel on which the title's and the text's first line's letters stand
text_baseline_row = 38
text_line_height = 16
title_color = "#202020"
text_color = "#404040"

[animation.banner]
kind = "fade"                  # "fade", "slide" (sideways, from and to the screen's side), "drop" (down from above and back up) or "none"
duration_ms = 150
curve = "ease-out"             # optional
```

Arriving and leaving can also be different. Describe each one like the table above:

```toml
[animation.banner]
arrive = { kind = "drop", duration_ms = 400, curve = "ease-out" }
leave = { kind = "slide", fade = true, duration_ms = 570, curve = "ease-in" }
```

How long a banner stays is behaviour, so it's in the desktop config (`[notifications] banner_seconds`).

## The list of past notifications

Once a banner has gone, its notification lands in a list. That's a panel at the screen's far side, sorted program by program. Its fonts are `[fonts].notification_list` (titles, in bold, and texts), `notification_list_heading` (the heading and the programs' names) and `notification_list_time`. `[animation.notification_list]` sets how it comes in (`kind`, `duration_ms`, `curve`):

- `"slide"`: the list slides in over the desktop.
- `"push"`: the list lies still at the screen's edge, and the desktop (its picture, icons and windows) slides aside to uncover it, while the panel and dock stay where they are. The list's `edge_shadow` runs along the desktop's edge as it goes. This needs the compositor's `desktop-aside` plugin, which the session loads.
- `"none"`: it's there at once.

```toml
[notifications.list]
width = 320
background = { color = "#f4f4f4" }       # any fill
edge_shadow = ["#00000018"]              # optional: colours of the columns at the edge towards the desktop, from the edge inwards
text_color = "#202020"
heading_color = "#20202070"
heading_height = 30
section_height = 26                      # the strip with a program's name
section_background = { color = "#e4e4e4" }
icon_left = 10
text_left = 34
title_top = 6                            # the title, below an entry's top
line_height = 16
time_right = 12
entry_bottom_padding = 8
divider_color = "#00000018"              # optional: a line between entries
empty_text_top = 200                     # where the single line of an empty list sits
close_image = "images/close.png"         # optional pictures: the close button,
close_pressed_image = "images/close-pressed.png"
close_right = 6                          #   its distance from the entry's end,
unread_image = "images/unread.png"       # and the mark before an unread entry
unread_left = 12
unread_top = 9
```

## Volume display

When you change the volume with a key, this shows the level on screen for a moment. A theme without `[volume_display]` doesn't have one. `[animation.volume_display]` sets how long it stays and how long it takes to fade (`stay_ms`, `fade_out_ms`).

```toml
[volume_display]
width = 200
height = 120
bottom = 120                   # from its lower edge to the screen's
background = { color = "#202020e0", radius = 10 }   # any fill
icon_size = 48                 # the icon theme's loudspeaker, at this size,
icon_top = 20                  # this far below the top,
icon_color = "#ffffff"         # in this colour

[volume_display.pictures]      # optional, instead of the icon: pictures as large as the whole display
volume = "images/volume.png"
muted = "images/volume-muted.png"

[volume_display.segments]      # the louder the output, the more of the `count` segments are filled
count = 16
width = 8
height = 6
pitch = 10                     # from one segment's left edge to the next's
left = 21                      # the first one's, from the display's left edge
top = 90
color = "#ffffff"
empty_color = "#ffffff40"
```

## Lock screen

While the session is locked, you see the desktop's background under a veil, and a box with the user's picture, their name and the password field. The name uses `[fonts].lock_name` and the field's text uses `[fonts].interface`.

A wrong password can set off `[animation.lock_wrong]`. Its `kind` is "shake" or "none". A shake takes `swing_ms`, the time of one half-swing, and then either:

- `swings`: how far the box goes on each swing (below zero is to the left). After the last one it comes to rest.
- or `distance` and `times`, for that many even swings to both sides.

```toml
[animation.lock_wrong]
kind = "shake"
swing_ms = 67
swings = [-14, 14, -14, 8, -8, 6]   # or:
distance = 12
times = 3
```

```toml
[lock]
text_color = "#202020"
backdrop_veil = { color = "#00000090" }   # any fill, laid over the background
name_baseline = 124            # how far below the box's top the name's baseline sits

[lock.box]
width = 280
height = 200
center_y = 0.45                # where the box's middle is, as a share of the screen's height
fill = { color = "#f4f4f4", radius = 8 }
shadow = { color = "#00000080", offset = [0, 6], blur = 12 }   # optional

[lock.picture]
size = 64
top = 28                       # below the box's top
round = true

[lock.field]
top = 148
width = 200
height = 26
fill = { color = "#ffffff", radius = 4, border = { width = 1, color = "#909090" } }
focus_rings = ["#3478f6"]      # optional: lines around the field, from its edge outwards
text_inset = 8
placeholder_color = "#909090"

[lock.clock]                   # optional: the time at the top right
right = 24
baseline = 18
color = "#ffffff8c"
```

## Application grid

A grid of all installed applications that covers the screen. The names use `[fonts].app_grid_label` and the search field's text uses `[fonts].interface`. `[animation.app_grid]` sets how the grid comes and goes: `kind` "fade" or "none", `duration_ms`, and optionally `close_duration_ms` if closing should take a different time than opening.

The pages sit side by side in a strip that follows a swipe on a touchpad or a drag. `[animation.app_grid_page]` sets how the strip settles on a page after a swipe, or after you turn the page with a key, the wheel or a dot (`kind` "slide" or "none", `duration_ms`, `curve`).

```toml
[app_grid]
background_blur = 16           # behind the grid is the desktop's background, blurred by about this many pixels (0: not at all)
background_veil = "#00000060"  # optional: a colour laid over that
columns = 6
rows = 4
column_pitch = 180             # from one icon's middle to the next
row_pitch = 140
columns_shift = 0              # optional: moves all columns sideways from an even spread about the screen's middle
first_row_top = 110            # the first row's icons' tops
icon_size = 64
label_baseline = 84            # how far below an icon's top its name's baseline sits
label_color = "#ffffff"
label_shadow_color = "#000000a0"   # optional: a dark edge under the names

[app_grid.search]              # the field at the top; typing narrows the icons
top = 40
width = 320
height = 32
background = { color = "#00000050", radius = 6 }   # any fill
icon = "images/search.png"     # optional
icon_inset = [8, 5]            # the icon's distance from the left and the top
text_left = 12
text_color = "#ffffff"

[app_grid.page_dots]           # one dot per page
size = 8
pitch = 18
center_above_bottom = 100
color = "#ffffff"              # the current page's dot
other_color = "#ffffff50"
```

The dock tile that opens the grid asks for the icon the config names for it. A theme can give its own picture for an icon name on tiles like that:

```toml
[dock.icons]
"view-app-grid" = "images/app-grid.svg"
```

## Window overview

This shows all windows side by side so you can pick one. It's the compositor's stock overview. This table holds what that overview lets a theme set, and all of it is optional. `[animation.window_overview] duration_ms` sets how long its motion takes.

```toml
[window_overview]
drawn_by = "shell"             # who draws it: "shell" (the fields under "The shell's overview" below) or
                               # "compositor" (the default; the compositor's own, with the fields here)
spacing = 30                   # between windows
outer_margin = 30              # between the windows and the screen's edges
other_windows_opacity = 0.85   # windows that are not the active one
titles = "all"                 # which windows get their title written over them: "all", "pointer" (the one under it) or "none"
title_font_size = 12
title_position = "bottom"      # "top", "center" or "bottom"
title_color = "#ffffff"
title_background = "#000000b0"
```

### The shell's overview

With `drawn_by = "shell"`, the desktop steps back onto a stage on a backdrop, and the windows are laid out in view. Each program's windows are grouped together, with its icon and name under them. The labels use `[fonts].window_overview_label`. `[animation.window_overview]` sets how long opening takes (`duration_ms`, `curve`).

```toml
[window_overview]
drawn_by = "shell"
backdrop = { color = "#2c333a" }      # any fill
stage_scale = 0.8                     # the desktop's picture, shrunk to this share of the screen
stage_top_share = 0.1                 # optional: where the stage's top is, as a share of the screen's height; without it the stage is centred
stage_shadow = { color = "#00000080", blur = 14 }   # optional
windows_top_share = 0.12              # the area the windows lie in: its top and bottom as shares of the
windows_bottom_share = 0.84           # screen's height, and its distance from the screen's sides
windows_side_margin = 40
most_window_scale = 0.8               # windows are never shown larger than this share of their size
group_gap = 40                        # between two programs' groups
group_icon_size = 32
group_icon_rise = 0                   # the icon's middle above the group's bottom edge
# Optional: a soft shadow for the icon to stand on. It's a picture from the
# theme, drawn `size` wide and high, with its middle `drop` below the icon's.
group_icon_shadow = { image = "images/icon-shadow.png", size = 96, drop = 4 }
group_name_gap = 4
label_color = "#ffffff"
label_shadow_color = "#000000b0"      # optional
selection = { color = "#00000000", radius = 4, border = { width = 3, color = "#3a6ea5" } }   # any fill: the mark of the window under the pointer
selection_reach = 4                   # how far the mark reaches beyond the window

[window_overview.strip]               # optional: small pictures of the desktops along the top
top = 22
picture_scale = 0.1
mark_width = 2                        # the line around the current desktop's picture
mark_color = "#c8c8c8"
name_gap = 14
slide = 30                            # how far the strip slides in from above
picture_gap = 80                      # between two desktops' pictures; the row stands in the screen's middle
# Optional: the button that removes a desktop. Once the pointer has rested
# on a picture, the button sits with its middle on the picture's top left corner.
close_button = { image = "images/close.png", pressed = "images/close-pressed.png", size = 30 }
close_delay_ms = 800
# Optional: the tile that adds a desktop. It's a paler desktop picture that
# comes out of the far top corner when the pointer gets near. Set how much of
# it comes into view, how pale it is, how fast it comes, and a picture on it.
add_tile = { image = "images/add.png", image_size = [24, 23], shown_share = 0.5, opacity = 0.6, slide_ms = 150 }
```

Without the strip, there's no way to add or remove a desktop in the overview. The number of desktops then comes from the config (`[desktops] count`).

## Screenshot flash

When you take a screenshot, the desktop can answer with a short flash (the config's `[screenshot] flash`). The theme sets how it looks:

```toml
[screenshot_flash]
color = "#ffffff"
opacity = 0.6          # how pale the screen goes at once

[animation.screenshot_flash]
duration_ms = 250      # how long it takes to clear
curve = "ease-out"
```

## Search

Search is a box that hangs from the panel near its far end. At the top is a strip with a label and a field. Below that are the results, with each kind's name in its own column on the left. The box itself is drawn like a menu's (`[menu]`: rim, shadow, corner radius), but square at the top.

Fonts: `[fonts].search` (the field's text and the results' names), `search_heading` (the kinds' names) and `search_label` (the label). Without them, it uses `menu`. The words, and what gets searched, come from the desktop config (`[search]`).

```toml
[search]
width = 420
right_inset = 8                          # from the box to the screen's edge
header_height = 40                       # the strip with label and field
header_background = { color = "#e4e4e4" }   # any fill
header_line_color = "#c0c0c0"            # optional: the strip's last row
header_text_color = "#202020"            # the label
header_text_shadow_color = "#00000040"   # optional: the label once more, one row lower
label_gap = 10                           # from the label's end to the column's end
heading_width = 110                      # the column for the label and the kinds' names
heading_gap = 10                         # from a kind's name to the column's end
heading_color = "#707070"
heading_column_color = "#ffffff"         # optional: the column's own colour under the strip
divider_color = "#dedede"                # optional: a line where the column ends
field_inset = 0                          # from the column's end to the field
field_right = 10                         # from the field to the box's edge
field_top = 7                            # optional; without it the field stands in the strip's middle
field_height = 26
field = { color = "#ffffff", radius = 4, border = { width = 1, color = "#b0b0b0" } }   # any fill
field_text_inset = 8
field_text_baseline_row = 18             # optional; without it the text stands in the field's middle
field_text_color = "#202020"
row_height = 24
row_gap = 0                              # between rows of one kind
kind_gap = 8                             # before a new kind
results_padding = 6                      # above the first row and under the last
highlight = { color = "#3a6ea5" }        # optional: the marked row; without it, the menus' highlight
highlight_text_color = "#ffffff"         # optional, likewise
icon_size = 16
icon_inset = 4                           # from the column's end to the icon
name_inset = 28                          # from the column's end to the name

[search.clear_button]                    # optional: the round button that empties the field
size = 14
right_inset = 4                          # from the button to the field's end
color = "#b0b0b0"
cross_color = "#ffffff"
cross_length = 7
cross_thickness = 1.5

[search.box]                             # optional: what differs from a menu's box, key by key
background = "#f6f6f6f7"
```

The panel item that opens the search asks for the icon `edit-find`. While the search is open, the item is drawn like an item with its menu open.
