# Settings, the Theme Installer, About This Computer and the App Store

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

How the window goes from one page to the next is up to `[animation.settings_page]`:

```toml
[animation.settings_page]
kind = "resize"     # "resize" or "none"
duration_ms = 200   # for a change of 150 points in height
```

With "resize", the window grows or shrinks to the new page's height while the page it leaves fades out over it. The time grows with the change in height, as AppKit times a window's resizing: `duration_ms` for every 150 points. Without the table, the window takes the new page's height at once.

## Theme Installer

The Theme Installer's window (`src/apps/theme-installer/`) has a column of steps on the left and the current step's page on the right. Go Back and Continue sit along the bottom and are drawn from `[dialog]`. Its fonts are `[fonts].theme_installer_heading` (each page's heading) and `theme_installer_text` (everything else).

After an update that gives a look something only a new import can bring, the Theme Installer opens once at login in a small window of its own. It lists what's new, one line each, with Later and Import Again, in the same background, fonts and buttons.

```toml
[theme_installer]
width = 640                     # the window's size
height = 500
news_width = 520                # the small window that offers a new import of a look: its width (its height follows its text),
news_icon_size = 64             # the installer's picture in it (drop_well_picture),
news_item_gap = 4               # and the gap between the features it lists
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

## App Store

The App Store (`src/apps/app-store/`) has a toolbar along the top: ◀ ▶ on the left, its tabs in the middle (Featured, Installed and Updates), and a search field on the right. Under it is the page that's showing. Featured is a grid of tiles: an app's icon, name and category, and a small button. An app's own page has its icon and a larger button on the left, its name, description and what's new in the middle, an Information column on the right, and its screenshots below. Installed and Updates list apps one row each.

The button goes from GET to INSTALL (`button_confirm`) when pressed once. While the app downloads, the button turns into a bar. Then it reads OPEN, or UPDATE when a newer version is out.

```toml
[fonts]
app_store_tab     = { family = "DejaVu Sans", size = 12 }               # the toolbar's tabs and Back
app_store_heading = { family = "DejaVu Sans", size = 15 }               # a heading over part of a page
app_store_title   = { family = "DejaVu Sans", size = 22 }               # the app's name on its page
app_store_name    = { family = "DejaVu Sans", size = 13 }               # an app's name in a tile or row
app_store_text    = { family = "DejaVu Sans", size = 12 }               # descriptions, messages
app_store_small   = { family = "DejaVu Sans", size = 11 }               # categories, versions, Information
app_store_button  = { family = "DejaVu Sans", size = 10, bold = true }  # GET, OPEN and the like

[app_store]
width = 960                          # the window's size when it opens
height = 660
background = "#ffffff"
text_color = "#202020"
heading_color = "#202020"            # the headings over parts of a page
secondary_text_color = "#808080"     # categories, versions, the Information labels
failure_text_color = "#c03030"       # what went wrong with an install
separator_color = "#dcdcdc"          # the lines under headings and rows
side_margin = 24                     # room at the pages' edges and between their columns
# The toolbar: its height, fill and the line under it, while the window is
# the active one and while it isn't. If the desktop config joins the window's
# title bar to it (joined_tool_bar_apps, docs/configuration.md), start these
# fills where the title bar's `background_joined` fills end, so the two look
# like one piece. The tab that's showing gets the `tab_selected` fill behind
# it. The Updates tab shows how many updates there are in a `badge`.
toolbar_height = 48
toolbar = { color = "#ececec" }
toolbar_inactive = { color = "#f4f4f4" }
toolbar_border_color = "#c0c0c0"
toolbar_border_color_inactive = "#d4d4d4"
toolbar_margin = 12                  # from the window's sides to ◀ ▶ and to the search field
toolbar_controls_top = 12            # optional: from the bar's top to ◀ ▶ and the search field; centred if left out
# ◀ ▶: one capsule, cut in two halves; the pressed half takes
# back_forward_pressed, and both take back_forward_inactive while the window
# isn't the active one. A line (back_forward_separator, this wide) parts them.
# The arrows are pictures, and greyed ones for a half with nowhere to go;
# without pictures they're triangles in arrow_color.
back_forward_size = [56, 24]
back_forward = { color = "#ffffff", radius = 4, border = { color = "#b0b0b0", width = 1 } }
back_forward_pressed = { color = "#e0e0e0", radius = 4, border = { color = "#b0b0b0", width = 1 } }
back_forward_inactive = { color = "#f8f8f8", radius = 4, border = { color = "#d0d0d0", width = 1 } }
back_forward_separator = { color = "#b0b0b0" }
back_forward_separator_width = 1
back_forward_separator_inset = [1, 1]   # rows the line leaves free at the capsule's top and bottom
back_glyph = "images/app_store/back.png"                       # optional
back_glyph_disabled = "images/app_store/back-disabled.png"     # optional
forward_glyph = "images/app_store/forward.png"                 # optional
forward_glyph_disabled = "images/app_store/forward-disabled.png"  # optional
arrow_color = "#404040"
# The search field, with a magnifier picture at its left (optional).
search_field_size = [160, 24]
search_field = { color = "#ffffff", radius = 12, border = { color = "#b0b0b0", width = 1 } }
search_icon = "images/app_store/search.png"
search_text_color = "#202020"
# The tabs, side by side in the middle of the bar. A tab is its name, or,
# with pictures, an icon with the name under it, like the items of an OS X
# toolbar; then make the tabs about as tall as the bar.
tab_height = 26
tab_padding = 12                     # room beside a tab's words, or its icon if that is wider
tab_spacing = 4                      # optional: between two tabs
tab_text_color = "#505050"
tab_text_color_inactive = "#909090"  # optional: every tab's name while the window isn't the active one
tab_selected_text_color = "#000000"
tab_selected = { color = "#d4d4d4", radius = 4 }
tab_selected_inactive = { color = "#e4e4e4", radius = 4 }   # optional: tab_selected while the window isn't the active one
# Optional: the tabs' icons, and greyed ones for while the window isn't the
# active one. With them, the icon's top is tab_icon_top under the tab's top,
# and the name's baseline tab_label_baseline under it (just under the icon
# if left out).
featured_glyph = "images/app_store/featured.png"
featured_glyph_disabled = "images/app_store/featured-disabled.png"
installed_glyph = "images/app_store/installed.png"
installed_glyph_disabled = "images/app_store/installed-disabled.png"
updates_glyph = "images/app_store/updates.png"
updates_glyph_disabled = "images/app_store/updates-disabled.png"
tab_icon_top = 6
tab_label_baseline = 44
badge = { color = "#3a6ea5", radius = 8 }
badge_text_color = "#ffffff"
heading_height = 36                  # a heading, with its line at the bottom
# Featured's tiles, side by side as many as fit.
tile_width = 290
tile_height = 96
tile_icon_size = 64
# An app's page.
page_icon_size = 128
information_width = 200
screenshot_height = 340              # the most a screenshot is shown at
# The rows of Installed and Updates.
row_height = 76
row_icon_size = 48
# The buttons: the page's, and the smaller one of tiles and rows. INSTALL
# uses the `button_confirm` fills; the bar while it downloads is
# `progress_bar` on `progress_track`.
button_height = 24
small_button_height = 20
button_padding = 12                  # room beside the words
button = { color = "#ffffff", radius = 4, border = { color = "#b0b0b0", width = 1 } }
button_pressed = { color = "#e0e0e0", radius = 4, border = { color = "#b0b0b0", width = 1 } }
button_text_color = "#202020"
button_confirm = { color = "#3a8a3a", radius = 4 }
button_confirm_pressed = { color = "#2e6e2e", radius = 4 }
button_confirm_text_color = "#ffffff"
progress_track = { color = "#e4e4e4", radius = 4 }
progress_bar = { color = "#3a6ea5", radius = 4 }
```

Every key is optional. One your theme leaves out gets the value above, and a font it leaves out is `[fonts] interface`. The pictures are the exception: without them, the arrows are drawn as triangles and the search field has no magnifier.
