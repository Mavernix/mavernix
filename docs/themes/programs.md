# Settings, the Theme Installer and About This Computer

## Settings window

The settings window (`src/apps/settings/`) is built from the toolkit's controls, so your theme's `[controls]` table dresses it. Its first page is its own, though. It shows the panes as icons with their names, in groups stacked as stripes one under the other. The config says which panes go in which group.

```toml
[settings.overview]
columns = 6                 # icons in a line; the window is as wide as they need
icon_size = 48              # the square an icon is drawn in
cell_pitch = 110            # from one icon's middle to the next
first_cell_middle = 65      # from the window's left to the first icon's middle (and the same at the right)
line_pitch = 104            # from one line of icons to the next within a group
group_bottom = 12           # room under a group's last line
icon_top = 14               # from a line's top to the icon's square
label_baseline = 82         # from a line's top to the baseline of the name's first line
label_line_pitch = 15       # to its second line
label_font_size = 12        # in the interface's face ([fonts] interface)
label_color = "#202020"
stripe = "#f6f6f6"          # the background of the first, third, … group
stripe_alternate = "#eeeeee"  # the second, fourth, …
# Under each group there's a line and a lighter line. The first colour of
# each list is used under a group on `stripe`, the second under one on
# `stripe_alternate`.
line_colors = ["#dcdcdc", "#dcdcdc"]
light_line_colors = ["#fafafa", "#fafafa"]
```

If your theme leaves the table out, it gets these numbers. All the text in the window uses `[fonts] interface`.

## Theme Installer

The Theme Installer's window (`src/apps/theme-installer/`) has a column of steps on the left and the current step's page on the right. Go Back and Continue sit along the bottom and are drawn from `[dialog]`. Its fonts are `[fonts].theme_installer_heading` (each page's heading) and `theme_installer_text` (everything else).

```toml
[theme_installer]
width = 640                     # the window's size
height = 500
background = { color = "#efefef" }
sidebar_width = 180             # the column of steps
sidebar = { color = "#e4e4e4" }
sidebar_line_color = "#d0d0d0"  # the line between the column and the page
steps_top = 36                  # where the first step sits, and how far apart the steps are
step_pitch = 30
step_inset = 22
step_marker_size = 10           # the dot before each step, and the gap after it
step_marker_gap = 10
step_text_color = "#7a7a7a"
step_current_text_color = "#202020"
step_marker_waiting = { color = "#00000000", radius = 5, border = { color = "#a8a8a8", width = 1 } }
step_marker_current = { color = "#3a6ea5", radius = 5 }
step_marker_done = { color = "#9a9a9a", radius = 5 }
content_margin = 22             # around the page
heading_height = 40
text_color = "#202020"
secondary_text_color = "#707070"
pane = { color = "#ffffff", radius = 4, border = { color = "#cccccc", width = 1 } }   # the box a page's text sits in
pane_padding = 18
paragraph_gap = 12
note = { color = "#fff6d6", radius = 4 }   # what's still missing before an import can start
drop_well = { color = "#f7f7f7", radius = 10, border = { color = "#b4b4b4", width = 2 } }
drop_well_ready = { color = "#eef3fa", radius = 10, border = { color = "#3a6ea5", width = 2 } }   # while something is held over it, or chosen
drop_well_height = 220
drop_well_icon_size = 64
drop_well_picture = "images/installer-disc.svg"   # the installer's picture in the well, a file in the theme
progress_height = 8
progress_track = { color = "#e2e2e2", radius = 4 }
progress_fill = { color = "#3a6ea5", radius = 4 }
stage_pitch = 24                # the rows of the import's stages
stage_waiting_color = "#a0a0a0"
stage_running_color = "#3a6ea5"
stage_done_color = "#3a8a3a"
stage_failed_color = "#c03030"
summary_mark_size = 44          # the large mark on the last page
button_bar_height = 56
disabled_button_opacity = 0.45  # how faint a button is when it can't be pressed
```

## About This Computer

This is the window that tells you what the computer is (`src/apps/about/`). From the top down, it shows a picture or a heading, one line about the system, a button, rows of a label and a fact, a second button, and some closing lines. The window is as tall as what it shows. The config's `[about]` decides the rows, the words, and whether a button with nothing to do is kept (`docs/configuration.md`).

```toml
[fonts]
about_heading = { family = "DejaVu Sans", size = 24, bold = true }   # the heading, when there's no picture
about_version = { family = "DejaVu Sans", size = 12 }                # the line under it
about_label   = { family = "DejaVu Sans", size = 12, bold = true }   # a row's label
about_value   = { family = "DejaVu Sans", size = 12 }                # a row's fact
about_button  = { family = "DejaVu Sans", size = 12 }
about_footer  = { family = "DejaVu Sans", size = 11 }                # the closing lines

[about]
width = 380
background = "#f4f4f4"
text_color = "#202020"
version_color = "#707070"        # optional; text_color if left out
footer_color = "#707070"         # optional; text_color if left out
# Optional: a picture at the top, centred. Without one, the config's heading
# goes there instead, with its box starting at heading_top.
picture = "images/about.png"
picture_size = [214, 124]
picture_top = 29
heading_top = 28
# Where the boxes start, counting from the window's top: the line about the
# system, the first button, the first row.
version_top = 66
update_button_top = 98
rows_top = 150
# Each row starts row_pitch after the one before and is row_height tall.
# The labels end in one column and the facts start row_gap after it. Both
# columns together sit in the middle of the window. A fact that would come
# closer than side_margin to the window's side is cut short.
row_pitch = 24
row_height = 16
row_gap = 8
side_margin = 24
# The gaps from the last row's end to the second button, from that button
# (or the last row, if there's no button) to the closing lines, between
# closing lines, and from the last line's end to the window's.
more_button_gap = 18
footer_gap = 18
footer_pitch = 16
bottom_gap = 18
# The buttons work like in [dialog]: fills, a height, the least width and
# the room beside the words. Optionally, the row the words sit on.
button_height = 28
button_width = 120
button_padding = 16
button = { color = "#ffffff", radius = 4, border = { color = "#b0b0b0", width = 1 } }
button_pressed = { color = "#e0e0e0", radius = 4, border = { color = "#b0b0b0", width = 1 } }
button_disabled = { color = "#f4f4f4", radius = 4, border = { color = "#d0d0d0", width = 1 } }
button_disabled_text_color = "#a0a0a0"
button_text_baseline_row = 16
```

If the first button is left out (the config has nothing for it to start and doesn't keep idle buttons), the rows start at `update_button_top`.
