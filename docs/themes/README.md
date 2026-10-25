# Theme format

A theme holds everything about how the desktop *looks and moves*: pictures, sizes, colours, fonts, animations, and the sound pack it goes with. It holds nothing about what the desktop *does*: which parts are shown, where they sit and how they behave belongs to the desktop config (`data/config/`, see [configuration](../configuration.md)).

The code does not know which theme is loaded. One theme comes with the desktop, `plain` (`data/themes/`), drawn without pictures; the Theme Installer makes `sea-lion` and `sea-lion-dark`, which are almost all pictures. All are read by the same code.

**The values in the examples below are made up to show the syntax.** They are not measurements of anything and must not be copied into a theme.

## Principles

1. **One format for every consumer.** The compositor plugins, the shell and the generators for GTK and Qt themes all read the same theme directory. A value is written once.
2. **Readable and editable by hand.** A theme is text plus pictures. Someone should be able to open it, change a number and see the result.
3. **Every look can be artwork or plain drawing.** Wherever a part can use a picture, it can instead use a colour, a gradient, a radius and a border. That is what lets `plain` exist without any images.
4. **Animations are data.** A theme chooses each animation's kind, duration and curve.
5. **Unknown keys are reported.** A misspelt key must not silently fall back to a default; `mavernix-check-theme` names it (see [Checking a theme](#checking-a-theme)). Missing optional keys fall back to documented defaults.

## Layout of a theme

```
NAME/
  theme.toml        all values
  images/           pictures the theme refers to, if any
    close.png
    close@2x.png
```

## Building on another theme

A theme may start from another and say only what differs:

```
based_on = "plain"

[panel]
background = { gradient = ["#c04040", "#902020"] }
```

Tables are joined key by key; a value written inline, such as a fill or a font, is taken whole from the theme above. Pictures are found in the theme that names them or, by the same path, in the one below it, so a theme replaces a picture of its base by holding a file of the same name. This is the quickest way to begin a new look: start from `plain`, then replace one part after another. A theme cannot take a key or a table away from its base.

`theme.toml` is [TOML](https://toml.io): plain text with comments, readable by Python's standard library and by maintained C++ libraries, which covers every program that needs it. (JSON was the alternative; it has no comments, which rules it out for something meant to be edited and annotated.)

## Units and scale

- All lengths are in **logical pixels**: the size at scale 1. At scale 2 everything is drawn twice as large.
- A picture `images/close.png` is the scale-1 version. If `images/close@2x.png` exists it is used at scale 2; otherwise the scale-1 picture is enlarged. The 2× picture must be exactly twice the size.
- Durations are in milliseconds, with keys ending in `_ms`.
- Colours are `"#rrggbb"` or `"#rrggbbaa"`.

## Top of the file

```toml
[theme]
name = "Plain"
description = "A flat, neutral desktop with no artwork."
format = 0                # the version of this format
```

## Fills: picture or plain

Any key documented as a *fill* takes one of these forms.

A plain colour:

```toml
background = { color = "#f0f0f0" }
```

A gradient, top to bottom. The colours are spread evenly unless `positions` places each one, from 0.0 (top) to 1.0 (bottom):

```toml
background = { gradient = ["#ebebeb", "#d2d2d2"] }
background = { gradient = ["#f0f0f0", "#d8d8d8", "#c0c0c0"], positions = [0.0, 0.3, 1.0] }
```

A picture that stretches. `slice` gives how many pixels at each edge stay fixed while the middle stretches (top, right, bottom, left), the same idea as CSS `border-image`:

```toml
background = { image = "images/titlebar.png", slice = [0, 6, 0, 6] }
```

A plain fill may add `radius` (corner rounding) and `border = { color = "...", width = 1 }`.

Any fill may add an `overlay`: a picture repeated across the whole area on top of it, for grain or texture. `opacity` runs from 0.0 to 1.0. `blend` says how the picture combines with what is underneath: `"normal"` (the default), `"overlay"` (darkens dark areas and lightens light ones while keeping the colour underneath, the usual way to add grain), `"multiply"` or `"screen"`.

```toml
background = { gradient = ["#f0f0f0", "#d0d0d0"], overlay = { image = "images/grain.png", opacity = 0.2, blend = "overlay" } }
```

*Not settled yet:* whether the stretching middle repeats or scales, per picture.

## States

Parts that change with interaction give one value per state. A state that is not given falls back to `normal`.

```toml
[window.buttons.close]
normal   = { image = "images/close.png" }
hover    = { image = "images/close-hover.png" }
pressed  = { image = "images/close-pressed.png" }
inactive = { image = "images/close-inactive.png" }   # window not focused
disabled = { image = "images/close-disabled.png" }

[window.buttons.minimize]
normal   = { image = "images/minimize.png" }
hover    = { image = "images/minimize-hover.png" }
pressed  = { image = "images/minimize-pressed.png" }
inactive = { image = "images/minimize-inactive.png" }
disabled = { image = "images/minimize-disabled.png" }

[window.buttons.maximize]
normal   = { image = "images/maximize.png" }
hover    = { image = "images/maximize-hover.png" }
pressed  = { image = "images/maximize-pressed.png" }
inactive = { image = "images/maximize-inactive.png" }
disabled = { image = "images/maximize-disabled.png" }   # does nothing here; else `inactive`
```

A button is `disabled` where it can do nothing: the maximize button of a window whose program fixed its size (its least and largest size are the same), which also has no edges to resize it by. Without the state it looks `inactive`.

## Fonts

Every text of the desktop has a font here; the pages of the parts say which is used where.

```toml
[fonts]
interface    = { family = "Lucida Grande", size = 13 }   # also for any part whose font is left out
window_title = { family = "Lucida Grande", size = 13 }
menu         = { family = "Lucida Grande", size = 14 }
panel        = { family = "Lucida Grande", size = 14 }
dock_label   = { family = "Lucida Grande", size = 14 }
dock_folder  = { family = "Lucida Grande", size = 12 }
desktop_icon = { family = "Lucida Grande", size = 12 }
program_switcher = { family = "Lucida Grande", size = 12 }
dialog_title = { family = "Lucida Grande", size = 13, bold = true }
dialog_text  = { family = "Lucida Grande", size = 11 }
banner_title = { family = "Lucida Grande", size = 13, bold = true }
banner_text  = { family = "Lucida Grande", size = 12 }
notification_list = { family = "Helvetica", size = 13 }
notification_list_heading = { family = "Helvetica", size = 14, bold = true }
notification_list_time = { family = "Helvetica", size = 11, bold = true }
app_grid_label = { family = "Lucida Grande", size = 12 }
window_overview_label = { family = "Lucida Grande", size = 13 }
lock_name    = { family = "Lucida Grande", size = 18, bold = true }
search       = { family = "Lucida Grande", size = 13 }
search_heading = { family = "Lucida Grande", size = 12 }
search_label = { family = "Lucida Grande", size = 13 }
theme_installer_heading = { family = "Lucida Grande", size = 15, bold = true }
theme_installer_text    = { family = "Lucida Grande", size = 13 }
```

Sizes are in **logical pixels**, like every other length in a theme, and not in points. A "point" means 1/72 inch to Linux toolkits but one screen pixel to some other systems, so "13 pt" would name two different sizes depending on who reads it; pixels have one meaning. (For reference: 13 pixels is what Linux toolkits call 9.75 pt at the usual 96 dots per inch.)

*Not settled yet:* text rendering options (hinting, subpixel, weight adjustment) and how far each toolkit can honour them.

## Parts

Each part of the desktop has a table of its own, described on its own page:

- [Window frame](window-frame.md)
- [Panel](panel.md)
- [Desktop](desktop.md)
- [Dock](dock.md)
- [Menus and dialogs](menus-and-dialogs.md)
- [Overlays](overlays.md): program switcher, notifications, volume display, lock screen, app grid, window overview, search
- [Settings and the Theme Installer](programs.md): the settings window's first page and the Theme Installer's window
- [Other applications](other-applications.md): icons, and the looks of Qt, GTK and single applications

## Sounds

A theme does not hold sounds itself; it names the sound pack that goes with it. Packs are folders of sound files named after events and can be exchanged without touching the theme ([sound packs](../sound-packs.md)). A theme without this table is silent unless the config names a pack.

```toml
[sounds]
pack = "SeaLion"     # a folder sounds/SeaLion/ among the data folders; a theme may bring its own pack in its share/sounds/
```

## Animations

Animations of windows have a table each under `[animation]`. One that a theme leaves out is switched off.

```toml
[animation.minimize]      # also used, backwards, for restoring
kind = "genie"
duration_ms = 500
curve = "ease-in-out"     # for "scale"

[animation.window_open]
kind = "fade"
duration_ms = 120
curve = "linear"

[animation.window_close]
kind = "fade"
duration_ms = 120
curve = "linear"

[animation.window_resize]
kind = "blend"
duration_ms = 150
curve = "ease-out"
```

- `kind` selects the effect; `"none"` is always allowed.
  - For `minimize`: `"genie"` (the window flows through a funnel into its place in the dock) or `"scale"` (it shrinks straight onto that place). Both do their own easing, so `curve` is not used. The place is the window's tile in the dock.
  - For `window_open` and `window_close`: `"fade"` or `"zoom"`, the compositor's own effects. If both use the same kind they must have the same timing.
  - For `window_resize`, a window changing to a new size at one go (made as large as the screen and back, or put on half of it): `"blend"`, the window's old picture stretched to the new size while the new one fades in over it. Without the table the change is at once.
- `curve` is one of `"linear"`, `"ease-in"`, `"ease-out"`, `"ease-in-out"`. The compositor has no true ease-in and uses ease-in-out for it.

### A dock that hides itself

```toml
[animation.dock_hide]
duration_ms = 240         # the slide below the screen's edge and back
curve = "ease-in-out"
show_delay_ms = 150       # the pointer rests at the screen's edge this long before the dock comes
hide_delay_ms = 300       # and has left the dock this long before it goes
```

Whether the dock hides itself is the user's choice (`[dock] autohide`). Without the table it goes and comes at once.

### Several desktops

```toml
[desktops]
gap = 64                  # the strip between two desktops while one gives way to the next; 0 for none
gap_color = "#000000"

[animation.desktop_switch]
duration_ms = 517
curve = [0.3, 0.525, 0.25, 1.0]
```

The desktops lie in a row and slide sideways. Without `[animation.desktop_switch]` the change is at once. Here, and so far only here, `curve` may also be four numbers: the two inner points of a cubic Bézier curve from (0, 0) to (1, 1), as CSS writes one. (The slide is the compositor's, which takes such a curve; the shell's own animations take the four names.)

### The shell's own parts

Each is described with its part; here is the whole list. Left out, the part comes and goes at once.

```toml
[animation.window_overview]   # overlays.md
duration_ms = 300
curve = "ease-out"

[animation.app_grid]          # overlays.md: "fade" or "none"
kind = "fade"
duration_ms = 200
close_duration_ms = 150

[animation.app_grid_page]     # overlays.md: "slide" or "none"
kind = "slide"
duration_ms = 300
curve = "ease-out"

[animation.dock_folder]       # dock.md: "grow", "fade" or "none"
kind = "grow"
duration_ms = 200
close_duration_ms = 150

[animation.dock_folder_fan]   # dock.md: "rise" or "none"
kind = "rise"
duration_ms = 200
close_duration_ms = 150

[animation.notification_list] # overlays.md: "slide" or "none"
kind = "slide"
duration_ms = 250
curve = "ease-out"

[animation.volume_display]    # overlays.md
stay_ms = 1000
fade_out_ms = 500
```

The dock's `[dock.launch_bounce]` and `magnification_ramp_ms`, the banners' `[animation.banner]`, the screenshot flash and the lock screen's shake are on their parts' pages.

## Checking a theme

The examples on these pages are the format: every key a theme may have is in one of them. `check_theme.py` holds a theme to them and names every key they do not have, which is almost always a misspelling:

```sh
mavernix-check-theme ~/.local/share/mavernix/themes/my-theme   # inside the session; outside it: PREFIX/lib/mavernix/session/bin/mavernix-check-theme
```

The tests run it on the plain theme, and the Theme Installer on every theme it makes. Besides, the compositor reads the window frame's tables when the session starts. A theme with an unknown or misspelt key, or a value of the wrong kind, is refused there with the key named in the message; the session's log (`$XDG_RUNTIME_DIR/mavernix/session.log`) has it, and the frames fall back to a built-in plain look. The tests check the two shipped themes the same way (`tests/test-theme-file.cpp`).

## What a theme must not contain

- Behaviour or layout (button side, dock position, which parts are enabled).
- Code or scripts.
- Anything a program has to guess the meaning of: every key is documented here.
