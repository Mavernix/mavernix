# Settings and the Theme Installer

## Settings window

The settings window (`src/apps/settings/`) is made of the toolkit's controls, which the theme's `[controls]` table dresses. Its first page is its own: the panes as icons with their names, in groups that lie under each other as stripes. The config says which panes are in which group.

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
stripe = "#f6f6f6"          # the first, third, … group's ground
stripe_alternate = "#eeeeee"  # the second, fourth, …
# Under each group a line and a lighter line; the first colour of each list
# is used under a group on `stripe`, the second under one on `stripe_alternate`.
line_colors = ["#dcdcdc", "#dcdcdc"]
light_line_colors = ["#fafafa", "#fafafa"]
```

A theme without the table gets these numbers. The words of the whole window are in `[fonts] interface`.

## Theme Installer

The Theme Installer's window (`src/apps/theme-installer/`): a column of steps at the left, the current step's page at the right, Go Back and Continue along the bottom, drawn from `[dialog]`. Its type is `[fonts].theme_installer_heading` (each page's heading) and `theme_installer_text` (everything else).

```toml
[theme_installer]
width = 640                     # the window's size
height = 500
background = { color = "#efefef" }
sidebar_width = 180             # the column of steps
sidebar = { color = "#e4e4e4" }
sidebar_line_color = "#d0d0d0"  # the line between the column and the page
steps_top = 36                  # where the first step stands, and how far apart they are
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
pane = { color = "#ffffff", radius = 4, border = { color = "#cccccc", width = 1 } }   # the box a page's text stands in
pane_padding = 18
paragraph_gap = 12
note = { color = "#fff6d6", radius = 4 }   # what is missing before an import can start
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
disabled_button_opacity = 0.45  # how faint a button is that cannot be pressed
```
