# Other applications

## Icons

```toml
[icons]
theme = "Adwaita"         # the name of an icon theme
```

Applications and the desktop's own parts (the dock) take icons by name from this icon theme. It may be one installed on the system, or one the desktop theme brings along: a theme's `share/` folder is added to the folders where programs look such things up, so `share/icons/NAME/` is found. A theme without this table uses the system's icons.

## Controls inside applications

How buttons, checkboxes and the like look in ordinary applications. No program of the desktop draws these; `src/session/make_app_theme.py` translates this table into themes for the applications' toolkits (Kvantum for Qt, CSS for GTK). A theme without this table leaves applications alone. What the table does not describe keeps a stock look.

```toml
[controls]
kvantum_base = "KvFlatLight"   # the stock Kvantum theme that supplies everything not described here
dark = false                   # optional: true for a dark theme, so that GTK starts from its dark variant
                               #   and programs that ask "is the desktop dark?" are told so
window_color = "#efefef"       # the body of windows and dialogs
text_color = "#202020"
base_color = "#ffffff"         # optional: the background of lists and text areas
highlight_color = "#3a6ea5"    # optional: what marks a selected item...
highlight_text_color = "#ffffff"   # ...and the text on it

[controls.tool_bar]            # optional: the bar of buttons under a window's title, drawn from colours
fill = ["#e8e8e8", "#d8d8d8"]  # one colour, or two running from top to bottom
top_line = "#f4f4f4"           # optional: one-pixel lines along its top...
bottom_line = "#a0a0a0"        # ...and bottom
fill_inactive = ["#f4f4f4", "#ececec"]   # optional: the same in a window in the background...
bottom_line_inactive = "#c8c8c8"         # ...and its bottom line; each as above if left out

[controls.selected_item]       # optional: the selected item of a list, drawn from colours
fill = ["#4a80b8", "#3a6ea5"]  # one colour, or two running from top to bottom
top_line = "#5a8cc0"           # optional: one-pixel lines along its top...
bottom_line = "#2f5c8c"        # ...and bottom
text_color = "#ffffff"
hover_fill = ["#00000014"]     # optional: the item under the pointer; without it, no mark

[controls.button]              # optional: push buttons, as pictures
# Toolkits take a picture's edge for the control's edge: they add the strips
# to the label's height and centre the label between them. So cut off any
# empty room around the visible button, or the button comes out too tall with
# its label off the middle; and choose the strips so that the label's middle
# lands where the original puts it.
slice = [5, 20, 5, 20]         # top, right, bottom, left strips that keep their size
normal = "images/controls/button.png"
pressed = "images/controls/button-pressed.png"
disabled = "images/controls/button-disabled.png"
default = "images/controls/button-default.png"            # the window's default button
default_pressed = "images/controls/button-default-pressed.png"

[controls.text_field]          # optional: fields to type in, as pictures
slice = [3, 2, 1, 2]
normal = "images/controls/text-field.png"
disabled = "images/controls/text-field-disabled.png"       # optional

[controls.tabs]                # optional: tabs and the pane they switch, as pictures (Qt only so far)
slice = [5, 6, 6, 6]
normal = "images/controls/tab.png"
pressed = "images/controls/tab-pressed.png"                # optional, like the next three
selected = "images/controls/tab-selected.png"
selected_pressed = "images/controls/tab-selected-pressed.png"
disabled = "images/controls/tab-disabled.png"
selected_text_color = "#ffffff"                            # optional
joined = false                 # optional: tabs touch, and only the row's two ends show the pictures' end strips
centered = false               # optional: the row is centred over the pane
on_pane_edge = false           # optional: the pane's top edge runs through the middle of the row
pane = "images/controls/tab-pane.png"                      # optional
pane_slice = [4, 4, 4, 4]

[controls.popup_button]        # optional: a button that opens a list of choices
slice = [5, 16, 7, 6]
normal = "images/controls/popup.png"
pressed = "images/controls/popup-pressed.png"              # optional
disabled = "images/controls/popup-disabled.png"            # optional
includes_arrows = true         # optional: the picture's right strip shows the mark; the toolkit's own arrow is left out
text_inset = 12                # optional: where the text starts, from the picture's left edge

[controls.progress_bar]        # optional
track = "images/controls/progress-track.png"               # the empty bar; its height is the bar's
track_slice = [1, 1, 2, 1]
fill = "images/controls/progress-fill.png"                 # shown inside the track's edge strips, over the filled length
shows_percentage = true                                    # optional: whether the bar may carry its number (GTK only; Kvantum has no such setting)

[controls.tool_button]         # optional: the buttons of a window's tool bar, as pictures (Qt only so far)
# Like [controls.button]: a picture's edge is the control's edge, so cut off
# empty room and choose strips that add up to the height the button should have.
slice = [4, 6, 4, 6]
normal = "images/controls/tool-button.png"
pressed = "images/controls/tool-button-pressed.png"        # optional, like the next one
disabled = "images/controls/tool-button-disabled.png"
# A theme without this table draws tool bar buttons plain: no frame, only the icon or text.

[controls.slider]              # optional
track = "images/controls/slider-track.png"                 # the groove; its height is the groove's
track_slice = [2, 3, 2, 3]
thumb = "images/controls/slider-thumb.png"                 # the knob, at its own size, centred on the groove
thumb_pressed = "images/controls/slider-thumb-pressed.png" # optional
thumb_disabled = "images/controls/slider-thumb-disabled.png"   # optional

[controls.checkbox]            # optional
size = [18, 18]
off = { normal = "…", pressed = "…", disabled = "…" }
on = { normal = "…", pressed = "…", disabled = "…" }
mixed = { normal = "…" }

[controls.radio]               # optional: the same, with off and on
size = [18, 18]
off = { normal = "…", pressed = "…", disabled = "…" }
on = { normal = "…", pressed = "…", disabled = "…" }
```

Pictures may come with an "@2x" version, as everywhere.

The colours here also reach KDE's programs, which take theirs from settings of their own rather than from the Qt style: the translator writes them a colour scheme with the window, list, selection and text colours.
