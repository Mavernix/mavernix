# Menus and dialogs

## Menus

These are the menus that drop down from the panel's titles, plus their submenus. The text uses `[fonts].menu`.

```toml
[menu]
background = "#fffffff2"
corner_radius = 5         # corners stay square where a menu hangs from the panel or its parent item
padding = 4               # above the first item and below the last

item_height = 19
text_baseline_row = 14    # the row of an item that the text's baseline sits on, counting from 1
text_inset = 21           # where an item's text starts
text_color = "#000000"
disabled_text_color = "#00000066"

separator_height = 12
separator_line_row = 7    # the row of a separator that its line sits on, counting from 1
separator_color = "#0000001a"

highlight = { image = "images/menu-highlight.png" }   # any fill; covers the selected item edge to edge
highlight_text_color = "#ffffff"

shortcut_end_inset = 11   # from the end of a shortcut (or submenu arrow) to the menu's edge
shortcut_gap = 20         # the least space between an item's text and its shortcut

# Optional: the shadow a menu casts, and a thin rim just outside its edge.
# The rim can also be a list of colours. That gives several lines, each one
# pixel further out than the one before.
shadow = { color = "#0000004d", offset = [0, 10], blur = 14 }
rim = "#00000024"
submenu_arrow = "▶"
check_mark = "✓"

# How the keys of a shortcut are written. A key that isn't listed is
# written by its name ("F1", "S").
[menu.key_symbols]             # any names
Control = "⌃"
Shift = "⇧"
Alt = "⌥"
Super = "⌘"
Right = "→"
```

The panel's table has a few more fields for how a title looks while its menu is open:

```toml
[panel]
title_highlight = { image = "images/panel-title-highlight.png" }   # any fill, over the panel's full height
title_highlight_text_color = "#ffffff"
logo_highlighted = "images/panel-logo-highlighted.png"   # optional: the logo while its menu is open
logo_highlight_overhang = 7   # optional: how far the highlight reaches past the logo on each side
```

What goes in the logo's menu is up to the config (`panel.logo_menu`).

The panel can also cast a shadow on whatever is next to it:

```toml
[panel]
shadow = { color = "#000000d9", blur = 16 }   # blur: how far it reaches, in pixels
```

The tick on a checked item and the arrow on an item with a submenu are plain characters (`check_mark`, `submenu_arrow`). If you'd rather, your theme can give shapes drawn from numbers instead:

```toml
[menu]
# A triangle pointing to the item's end. You set its size, its colour while
# the item isn't selected (the item's text colour if left out), how far
# before the menu's edge its point ends, and how far above the label's
# baseline its middle sits.
submenu_arrow_shape = { width = 7, height = 9, color = "#404040", end_inset = 13, middle_above_baseline = 5 }
# A tick made of two strokes, from `start` to `bend` to `end`, inside a box
# of `width` by `height` that starts `left_inset` from the menu's edge.
check_mark_shape = { width = 10, height = 11, left_inset = 6, stroke = 2, color = "#404040", start = [0.5, 5.0], bend = [4.5, 9.5], end = [9.2, 0.3], middle_above_baseline = 4 }
```

## Dialogs

This is about the shell's own dialogs: the one that asks before you shut down, restart or log out. (Dialogs in applications belong to their toolkits, see `[controls]`.) The dialog's frame, shadow and corner rounding come from `[window.*]`. Its two fonts are `[fonts].dialog_title` and `[fonts].dialog_text`, and its buttons use `[fonts].interface`.

```toml
[dialog]
width = 440
title_bar = false              # an empty title bar along the top, in the window frame's look
background = "#f4f4f4"
text_color = "#202020"
icon_size = 48
icon_inset = [20, 20]          # the picture's distance from the left and the top
text_left = 84                 # where the text column's box starts, and how wide it is;
text_width = 336               #   a text's ink starts 2 inside its box
title_top = 20                 # the question's distance from the top
title_line_height = 18
text_gap = 8                   # between the question and the lines under it
text_line_height = 16
button_gap_above = 16          # between the text (or the picture, if it ends lower) and the buttons
button_inset = [16, 16]        # the buttons' distance from the right and the bottom
button_spacing = 8             # optional: between two buttons
button_padding = 16            # at each end of a button's label
button_min_width = 88
button_height = 28
button = { color = "#ffffff", radius = 4, border = { color = "#b0b0b0", width = 1 } }   # any fill
button_pressed = { color = "#e0e0e0", radius = 4, border = { color = "#b0b0b0", width = 1 } }
button_default = { color = "#3a6ea5", radius = 4 }           # the button the Return key presses
button_default_pressed = { color = "#2f5c8c", radius = 4 }
button_default_text_color = "#ffffff"                        # optional
button_text_baseline_row = 19  # optional: the row of the button's box the label sits on; centred without it
top_share = 0.35               # how much of the free height lies above the dialog; 0.5 centres it

[dialog.icons]                 # any names; optional: your theme's own picture for an icon name
"system-shutdown" = "images/dialog/shut-down.png"
```

A button's fill can be a picture with empty room around the visible button (for its shadow). In that case, sizes and insets count the whole picture, and two such buttons touch.

The dialog's words, its icon and whether it counts down are set in the desktop config, along with the menu entry that opens it. The icon is given by name: your theme's own picture from `[dialog.icons]` if there is one, otherwise the icon theme's.
