# Overlays

## Program switcher

The plate that shows the programs with windows while the switching key is held (`src/shell/ProgramSwitcher.qml`). Without the table there is no switcher.

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
# room beside the name).
name = { color = "#202020", gap = 10, padding = 6 }
appear_after_ms = 150               # the plate shows only once the key has been held this long
```

## Notifications

The banner in which a program's notification shows up for a few seconds, in the screen's top corner at the end side. Its fonts are `[fonts].banner_title` and `[fonts].banner_text`; how it comes and goes is `[animation.banner]`.

```toml
[notifications.banner]
width = 340
top_gap = 10                   # below the panel (or the screen's top, if the panel is elsewhere)
edge_gap = 10                  # from the screen's side
gap = 8                        # between two banners
# One background per number of text lines under the title (none, one, two),
# each any fill, and the banner's height for each.
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

Coming and going may also differ, each described like the table above:

```toml
[animation.banner]
arrive = { kind = "drop", duration_ms = 400, curve = "ease-out" }
leave = { kind = "slide", fade = true, duration_ms = 570, curve = "ease-in" }
```

How long a banner stays is behaviour and is in the desktop config (`[notifications] banner_seconds`).

## The list of past notifications

A panel at the screen's far side that lists the notifications whose banners have gone, program by program. Fonts: `[fonts].notification_list` (titles, in bold, and texts), `notification_list_heading` (the heading and the programs' names), `notification_list_time`. It slides in as `[animation.notification_list]` says (`kind` "slide" or "none", `duration_ms`, `curve`).

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
empty_text_top = 200                     # where the one line of an empty list stands
close_image = "images/close.png"         # optional pictures: the close button,
close_pressed_image = "images/close-pressed.png"
close_right = 6                          #   its distance from the entry's end,
unread_image = "images/unread.png"       # and the mark before an unread entry
unread_left = 12
unread_top = 9
```

## Volume display

What shows the loudness on screen for a moment after it was changed by key. A theme without `[volume_display]` has none. How long it stays and how long it takes to fade is `[animation.volume_display]` (`stay_ms`, `fade_out_ms`).

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

[volume_display.segments]      # as many of `count` are filled as the output is loud
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

What is shown while the session is locked: the desktop's background under a veil, and a box with the user's picture and name and the field for the password. The name's font is `[fonts].lock_name`, the field's text `[fonts].interface`. A wrong password may set off `[animation.lock_wrong]`: `kind` "shake" or "none"; for a shake `swing_ms`, the time of one half-swing, and either `swings`, how far the box goes at each (below zero to the left; after the last it comes to rest), or `distance` and `times` for as many even swings to both sides.

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
name_baseline = 124            # the row below the box's top on which the name stands

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

The grid of all installed applications that covers the screen. Its names' font is `[fonts].app_grid_label`, the search field's text `[fonts].interface`; how it comes and goes is `[animation.app_grid]` (`kind` "fade" or "none", `duration_ms`, and optionally `close_duration_ms` if going takes a different time from coming). Its pages stand side by side in a strip that follows a pull on a touchpad or a drag; how the strip comes to rest on a page, after a pull or a turn by key, wheel or dot, is `[animation.app_grid_page]` (`kind` "slide" or "none", `duration_ms`, `curve`).

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
label_baseline = 84            # the row below an icon's top on which its name's letters stand
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
color = "#ffffff"              # the shown page's
other_color = "#ffffff50"
```

The dock's tile that opens the grid asks for the icon the config names for it; a theme can give its own picture for an icon name on such tiles:

```toml
[dock.icons]
"view-app-grid" = "images/app-grid.svg"
```

## Window overview

The view of all windows side by side, to pick one. It is the compositor's stock overview; this table holds what that lets a theme set, and all of it is optional. How long its motion takes is `[animation.window_overview] duration_ms`.

```toml
[window_overview]
drawn_by = "shell"             # who draws it: "shell" (the fields under "The shell's overview" below) or
                               # "compositor" (the default; the compositor's own, with the fields here)
spacing = 30                   # between windows
outer_margin = 30              # between the windows and the screen's edges
other_windows_opacity = 0.85   # windows that are not the active one
titles = "all"                 # whose title is written over its window: "all", "pointer" (the one under it) or "none"
title_font_size = 12
title_position = "bottom"      # "top", "center" or "bottom"
title_color = "#ffffff"
title_background = "#000000b0"
```

### The shell's overview

With `drawn_by = "shell"` the desktop steps back onto a stage on a backdrop, and the windows are laid out in view, each program's together with its icon and name under them. Fonts: `[fonts].window_overview_label`. How long opening takes: `[animation.window_overview]` (`duration_ms`, `curve`).

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
group_gap = 40                        # between the groups of two programs
group_icon_size = 32
group_icon_rise = 0                   # the icon's middle above the group's bottom edge
# Optional: a soft shadow the icon stands on, a picture of the theme's drawn
# `size` wide and high with its middle `drop` below the icon's.
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
# Optional: the button that takes a desktop away, its middle on the
# picture's top left corner once the pointer has rested there.
close_button = { image = "images/close.png", pressed = "images/close-pressed.png", size = 30 }
close_delay_ms = 800
# Optional: the tile that adds a desktop, a paler desktop picture that comes
# out of the far top corner when the pointer is near: the share of it that
# comes into view, how pale it is, how fast it comes, and a picture on it.
add_tile = { image = "images/add.png", image_size = [24, 23], shown_share = 0.5, opacity = 0.6, slide_ms = 150 }
```

A theme without the strip has no way of adding or removing a desktop in the overview; the number is then the config's (`[desktops] count`).

## Screenshot flash

A desktop may answer a picture of the screen with a short flash (the config's `[screenshot] flash`). How it looks:

```toml
[screenshot_flash]
color = "#ffffff"
opacity = 0.6          # how pale the screen goes at once

[animation.screenshot_flash]
duration_ms = 250      # how long it takes to clear
curve = "ease-out"
```

## Search

A box that hangs from the panel near its far end: a strip with a label and a field, and under it what was found, each kind's name in a column of its own at the left. The box itself is drawn as a menu's (`[menu]`: rim, shadow, corner radius; square at the top). Fonts: `[fonts].search` (the field's text and the results' names), `search_heading` (the kinds' names), `search_label` (the label); without them, `menu`. The words, and what is searched, are the desktop config's (`[search]`).

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

The panel's item that opens the search asks for the icon `edit-find`; while the search is open the item is drawn as an item with its menu open.
