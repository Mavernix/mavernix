# Theme format

A theme decides how the desktop *looks and moves*: pictures, sizes, colours, fonts, animations, and the sound pack that goes with it. It doesn't decide what the desktop *does*. Which parts you see, where they sit and how they behave all belong in the desktop config (`data/config/`, see [configuration](../configuration.md)).

The code doesn't know which theme is loaded. The desktop comes with one theme, `plain` (`data/themes/`), which uses no pictures at all. The Theme Installer makes two more, `sea-lion` and `sea-lion-dark`, which are almost all pictures. The same code reads all of them.

**The values in the examples below are made up to show the syntax.** They aren't measurements of anything, so don't copy them into a theme.

## Principles

1. **One format for everything that reads it.** The compositor plugins, the shell and the GTK and Qt theme generators all read the same theme directory. You write each value once.
2. **You can read and edit it by hand.** A theme is text plus pictures. You should be able to open it, change a number and see what happens.
3. **Every look can be artwork or plain drawing.** Anywhere a part can use a picture, it can use a colour, a gradient, a radius and a border instead. That's how `plain` gets by without any images.
4. **Animations are data.** The theme picks each animation's kind, duration and curve.
5. **Unknown keys get reported.** A misspelt key shouldn't quietly fall back to a default. `mavernix-check-theme` points it out (see [Checking a theme](#checking-a-theme)). Optional keys you leave out fall back to the defaults documented here.

## Layout of a theme

```
NAME/
  theme.toml        all values
  images/           pictures the theme refers to, if any
    close.png
    close@2x.png
```

## Building on another theme

A theme can start from another one and only say what's different:

```
based_on = "plain"

[panel]
background = { gradient = ["#c04040", "#902020"] }
```

Tables are merged key by key. A value written inline, like a fill or a font, is taken whole from the theme on top. A picture is looked up in the theme that names it, then at the same path in the theme below. So to replace a picture from the base, just put a file with the same name in your theme.

This is the quickest way to start a new look: begin with `plain`, then replace one part at a time. You can't remove a key or a table that the base theme has, though.

`theme.toml` is [TOML](https://toml.io). It's plain text with comments, and Python's standard library and maintained C++ libraries can read it, which covers every program that needs to. (The other option was JSON. It has no comments, and that rules it out for a file people are meant to edit and annotate.)

## Units and scale

- All lengths are in **logical pixels**, meaning the size at scale 1. At scale 2 everything is drawn twice as large.
- A picture like `images/close.png` is the scale-1 version. If there's also an `images/close@2x.png`, it's used at scale 2. If not, the scale-1 picture is enlarged. The 2× picture has to be exactly twice the size.
- Durations are in milliseconds, with keys ending in `_ms`.
- Colours are `"#rrggbb"` or `"#rrggbbaa"`.

## Top of the file

```toml
[theme]
name = "Plain"
description = "A flat, neutral desktop with no artwork."
format = 0                # the version of this format
revision = 1              # optional: which update of its look the Theme Installer made it from
updated_to = 2            # optional: the update whose settings it was last given at login
```

The Theme Installer writes `revision` into the themes it makes. When a newer version of the desktop brings settings a theme can take as it is, they're added to older themes once, at login, and `updated_to` notes it. A value you change afterwards stays. When an update brings something only a new import can give, the Theme Installer offers that import once.

## Fills: picture or plain

Wherever a key is documented as a *fill*, you can use any of these forms.

A plain colour:

```toml
background = { color = "#f0f0f0" }
```

A gradient, top to bottom. The colours are spread evenly, unless you place each one with `positions`, from 0.0 (top) to 1.0 (bottom):

```toml
background = { gradient = ["#ebebeb", "#d2d2d2"] }
background = { gradient = ["#f0f0f0", "#d8d8d8", "#c0c0c0"], positions = [0.0, 0.3, 1.0] }
```

A picture that stretches. `slice` says how many pixels at each edge stay fixed while the middle stretches (top, right, bottom, left). It's the same idea as CSS `border-image`:

```toml
background = { image = "images/titlebar.png", slice = [0, 6, 0, 6] }
```

A plain fill can also have a `radius` (corner rounding) and a `border = { color = "...", width = 1 }`.

Any fill can also have an `overlay`. That's a picture tiled across the whole area on top of the fill, for grain or texture. `opacity` runs from 0.0 to 1.0. `blend` says how the picture mixes with what's underneath:

- `"normal"` (the default)
- `"overlay"` darkens dark areas and lightens light ones but keeps the colour underneath. This is the usual way to add grain.
- `"multiply"`
- `"screen"`

```toml
background = { gradient = ["#f0f0f0", "#d0d0d0"], overlay = { image = "images/grain.png", opacity = 0.2, blend = "overlay" } }
```

*Not settled yet:* whether the stretching middle repeats or scales, per picture.

## States

Parts that change as you use them take one value per state. Any state you leave out falls back to `normal`.

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

A button is `disabled` when it can't do anything. For example, the maximize button of a window whose program fixed its size (its smallest and largest size are the same). Such a window has no edges to resize it by either. If you leave the state out, the button looks `inactive`.

## Fonts

Every bit of text on the desktop gets its font from here. Each part's page says which font it uses where.

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

Sizes are in **logical pixels**, like every other length in a theme, not in points. To Linux toolkits a "point" is 1/72 inch, but to some other systems it's one screen pixel. So "13 pt" could mean two different sizes depending on who reads it. Pixels only mean one thing. (For reference, 13 pixels is what Linux toolkits call 9.75 pt at the usual 96 dots per inch.)

*Not settled yet:* text rendering options (hinting, subpixel, weight adjustment) and how far each toolkit can honour them.

## Parts

Each part of the desktop has its own table, and its own page:

- [Window frame](window-frame.md)
- [Panel](panel.md)
- [Desktop](desktop.md)
- [Dock](dock.md)
- [Menus and dialogs](menus-and-dialogs.md)
- [Overlays](overlays.md): program switcher, notifications, volume display, lock screen, app grid, window overview, search
- [Settings and the Theme Installer](programs.md): the settings window's first page and the Theme Installer's window
- [Other applications](other-applications.md): icons, and the looks of Qt, GTK and single applications

## Sounds

A theme doesn't contain sounds. It just names the sound pack that goes with it. A pack is a folder of sound files named after events, and you can swap packs without touching the theme ([sound packs](../sound-packs.md)). If a theme has no `[sounds]` table, it's silent unless the config names a pack.

```toml
[sounds]
pack = "SeaLion"     # a folder sounds/SeaLion/ among the data folders; a theme may bring its own pack in its share/sounds/
```

## Animations

Each window animation has its own table under `[animation]`. Any animation the theme leaves out is switched off.

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

- `kind` picks the effect. `"none"` always works.
  - For `minimize`: `"genie"` (the window pours through a funnel into its place in the dock) or `"scale"` (it shrinks straight onto that place). That place is the window's tile in the dock. Both effects do their own easing, so `curve` isn't used.
  - For `window_open` and `window_close`: `"fade"` or `"zoom"`, which are the compositor's own effects. If both use the same kind, they have to have the same timing.
  - For `window_resize`: `"blend"`. This is for a window that jumps to a new size in one go, like when it's made as large as the screen and back, or put on half of it. The window's old picture is stretched to the new size while the new one fades in over it. Without the table, the change is instant.
- `curve` is one of `"linear"`, `"ease-in"`, `"ease-out"`, `"ease-in-out"`. The compositor has no real ease-in, so it uses ease-in-out instead.

### A dock that hides itself

```toml
[animation.dock_hide]
duration_ms = 240         # the slide below the screen's edge and back
curve = "ease-in-out"
show_delay_ms = 150       # how long the pointer rests at the screen's edge before the dock comes up
hide_delay_ms = 300       # how long the pointer is off the dock before it goes away
```

Whether the dock hides itself is up to the user (`[dock] autohide`). Without the table, the dock goes and comes instantly.

### Several desktops

```toml
[desktops]
gap = 64                  # the strip between two desktops while one slides away for the next; 0 for none
gap_color = "#000000"

[animation.desktop_switch]
duration_ms = 517
curve = [0.3, 0.525, 0.25, 1.0]
```

The desktops sit in a row and slide sideways. Without `[animation.desktop_switch]`, switching is instant.

Here, and so far only here, `curve` can also be four numbers. They're the two inner points of a cubic Bézier curve from (0, 0) to (1, 1), written the way CSS writes one. (The compositor does this slide, and it takes such a curve. The shell's own animations only take the four names.)

### The shell's own parts

Each of these is described on its part's page, but here's the full list. If you leave one out, that part appears and disappears instantly.

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

You'll find the dock's `[dock.launch_bounce]` and `magnification_ramp_ms`, the banners' `[animation.banner]`, the screenshot flash and the lock screen's shake on their parts' pages.

## Checking a theme

The examples on these pages *are* the format. Every key a theme can have shows up in one of them. `check_theme.py` compares your theme against them and lists every key they don't have. That's almost always a typo:

```sh
mavernix-check-theme ~/.local/share/mavernix/themes/my-theme   # inside the session; outside it: PREFIX/lib/mavernix/session/bin/mavernix-check-theme
```

The tests run it on the plain theme, and the Theme Installer runs it on every theme it makes.

The compositor also reads the window frame's tables when the session starts. If it finds an unknown or misspelt key, or a value of the wrong kind, it refuses the theme and names the key in its message. You'll find that in the session's log (`$XDG_RUNTIME_DIR/mavernix/session.log`), and the frames fall back to a built-in plain look. The tests check the two shipped themes the same way (`tests/test-theme-file.cpp`).

## What a theme must not contain

- Behaviour or layout (button side, dock position, which parts are enabled).
- Code or scripts.
- Anything a program would have to guess the meaning of. Every key is documented here.
