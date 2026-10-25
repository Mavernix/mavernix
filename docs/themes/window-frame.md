# Window frame

```toml
[window.titlebar]
height = 22
background          = { gradient = ["#ebebeb", "#d2d2d2"] }
background_inactive = { color = "#f6f6f6" }
# Optional thin lines under the bar, one pixel each, between the bar and the
# window's content. They add to the frame's height (a bar of 22 with one
# line takes 23 rows). The first colour is the lowest line. The bar's fill
# continues behind them, so a translucent line tints the fill.
separator           = ["#00000040"]
separator_inactive  = ["#00000020"]
# Optional thin lines along the top of the bar, drawn over its first rows.
highlight           = ["#ffffff60"]
highlight_inactive  = ["#ffffff40"]
# Optional, each the same as the plain key if left out: the bar of a window
# whose program draws a tool bar of its own directly under it (the desktop
# config's joined_tool_bar_apps names them), so that bar and tool bar look
# like one piece.
background_joined          = { gradient = ["#e8e8e8", "#d1d1d1"] }
background_joined_inactive = { gradient = ["#f6f6f6", "#e8e8e8"] }
separator_joined           = ["#00000000"]     # no line between bar and tool bar
separator_joined_inactive  = ["#00000000"]
title_color          = "#404040"
title_color_inactive = "#a0a0a0"
title_align = "center"                 # "left", "center" or "right"
title_padding = 8                      # space kept free at both ends of the title

[window.buttons]
# Order and side are behaviour and live in the config. The theme only says
# how big the buttons are, how far apart they sit and how far from the edge.
size = [16, 16]          # width and height of each button's box
spacing = 8
edge_inset = 8
hover_together = true    # optional: all buttons show their hover state while the pointer is over any one

[window.frame]
border_width = 1                    # line around the window; 0 for none
border_color          = "#a0a0a0"
border_color_inactive = "#c0c0c0"
corner_radius = 4                   # rounding of the top corners
# Optional (0, square, if left out): rounding of the bottom corners. Below
# the title bar a window is the program's own picture; the compositor cuts
# its corners round, and the shadow and rim follow the rounding. Meant for
# frames without a border (`border_width = 0`): a border stays square.
bottom_corner_radius = 4
# Optional (4 if left out): how far outside the window's edge a press still
# takes hold of the edge to resize the window. The strip is not drawn. It is
# what makes a window with `border_width = 0` resizable by its edges.
resize_grip = 4
# Optional (0 if left out): how many rows at the title bar's top take hold
# of the top edge too, instead of moving the window. (The other edges have
# the program's own content inside them, which stays the program's.)
resize_grip_in_title_bar = 0
# Optional (0 if left out): how far from a corner, along either edge, a
# press takes hold of both edges.
resize_corner = 16
# Optional: a hairline just outside the window, one pixel wide, drawn over
# whatever is next to the window and taking no room. Three colours: above,
# beside and below the window.
rim          = ["#00000026", "#00000042", "#00000054"]
rim_inactive = ["#0000001c", "#0000002b", "#00000029"]

[window.snap_preview]             # optional: the outline that shows where a window dragged to the
color = "#3a6ea540"               # screen's side will go (if the config has windows snap there)
border_color = "#3a6ea5cc"
border_width = 2

[window.shadow]                   # optional; without it windows cast no shadow
color = "#00000080"               # its opacity is how dark the shadow is at its darkest
blur_radius = 20                  # softness, as in CSS: twice the blur's standard deviation
offset = [0, 10]                  # moved right and down by this
# For inactive windows; each defaults to the value above.
color_inactive = "#00000040"
blur_radius_inactive = 12
offset_inactive = [0, 5]
```
