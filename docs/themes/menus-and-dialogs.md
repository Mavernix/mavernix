# Menus and dialogs

## Menus

The menus that drop from the panel's titles, and their submenus. The text's font is `[fonts].menu`.

```toml
[menu]
background = "#fffffff2"
corner_radius = 5         # corners are square where a menu hangs from the panel or its parent item
padding = 4               # above the first and below the last item

item_height = 19
text_baseline_row = 14    # the row of an item the text's baseline is on, counted from 1
text_inset = 21           # where an item's text starts
text_color = "#000000"
disabled_text_color = "#00000066"

separator_height = 12
separator_line_row = 7    # the row of a separator its line is on, counted from 1
separator_color = "#0000001a"

highlight = { image = "images/menu-highlight.png" }   # any fill; covers the selected item edge to edge
highlight_text_color = "#ffffff"

shortcut_end_inset = 11   # from the end of a shortcut (or submenu arrow) to the menu's edge
shortcut_gap = 20         # least space between an item's text and its shortcut

# Optional: the shadow a menu casts, and a thin rim just outside its edge.
# The rim may also be a list of colours: several lines, each one pixel
# further out than the one before.
shadow = { color = "#0000004d", offset = [0, 10], blur = 14 }
rim = "#00000024"
submenu_arrow = "▶"
check_mark = "✓"

# How the keys of a shortcut are written. A key not listed is written by
# its name ("F1", "S").
[menu.key_symbols]             # any names
Control = "⌃"
Shift = "⇧"
Alt = "⌥"
Super = "⌘"
Right = "→"
```

In the panel's table, two more fields say how a title looks while its menu is open:

```toml
[panel]
title_highlight = { image = "images/panel-title-highlight.png" }   # any fill, over the panel's full height
title_highlight_text_color = "#ffffff"
logo_highlighted = "images/panel-logo-highlighted.png"   # optional: the logo while its menu is open
logo_highlight_overhang = 7   # optional: how far the highlight reaches beyond the logo, each side
```

What the logo's menu holds is the config's business (`panel.logo_menu`).

A panel may cast a shadow on what lies next to it:

```toml
[panel]
shadow = { color = "#000000d9", blur = 16 }   # blur: how far it reaches, in pixels
```

The mark of a ticked item and of an item with a submenu are characters (`check_mark`, `submenu_arrow`), or, if the theme gives these, shapes drawn from numbers:

```toml
[menu]
# A triangle pointing to the item's end: its size, its colour while the item
# is not marked (the item's text colour if left out), how far before the
# menu's edge its point ends, and how far above the label's baseline its
# middle is.
submenu_arrow_shape = { width = 7, height = 9, color = "#404040", end_inset = 13, middle_above_baseline = 5 }
# A tick of two strokes inside a box of `width` by `height` that begins
# `left_inset` from the menu's edge: from `start` to `bend` to `end`.
check_mark_shape = { width = 10, height = 11, left_inset = 6, stroke = 2, color = "#404040", start = [0.5, 5.0], bend = [4.5, 9.5], end = [9.2, 0.3], middle_above_baseline = 4 }
```

## Dialogs

The shell's own dialogs: the one that asks before shutting down, restarting or logging out. (Dialogs of applications are the toolkits' business, `[controls]`.) Its frame, shadow and corner rounding are those of `[window.*]`; its two fonts are `[fonts].dialog_title` and `[fonts].dialog_text`, its buttons' `[fonts].interface`.

```toml
[dialog]
width = 440
title_bar = false              # an empty title bar of the window frame's look along the top
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
button_gap_above = 16          # between the text (or the picture, if that ends lower) and the buttons
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
button_text_baseline_row = 19  # optional: the row of the button's box the label stands on; without it, centred
top_share = 0.35               # the share of the free height that lies above the dialog; 0.5 centres it

[dialog.icons]                 # any names; optional: a picture of the theme's own for an icon name
"system-shutdown" = "images/dialog/shut-down.png"
```

A button's fill may be a picture that has empty room around the visible button (for its shadow). Sizes and insets then count the whole picture, and two such buttons touch.

The dialog's words, its icon (by name: the theme's own picture from `[dialog.icons]`, else the icon theme's) and whether it counts down are in the desktop config, with the menu entry that opens it.
