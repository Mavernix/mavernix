# Where this code comes from

This plugin started as a copy of the window decoration plugin that ships with Wayfire, so that it builds on maintained, working code.

| | |
|---|---|
| Origin | [Wayfire](https://github.com/WayfireWM/wayfire), `plugins/decor/` and `metadata/decoration.xml` |
| Version | 0.11.0, tag `v0.11.0` |
| Licence | MIT, copyright Iliya Bozhinov and Wayfire's contributors. The text is in `LICENSE-wayfire` and stays with this code |

## Changes from the original

Kept current, so that the code can be compared with a newer Wayfire release.

1. Renamed from `decoration` to `themed-decoration`, including the prefix of every option, so that it can be installed beside the original.
2. Built outside Wayfire's source tree.
3. **Looks come from a theme directory.** New units `theme-file` (reads `theme.toml` into checked values) and `fill-painter` (draws plain, gradient and picture fills); `deco-theme` was rewritten around them. The original's colour, font and size options are gone; the one new option is `theme`, the theme directory.
4. **Title bar:** drawn as one picture across the frame's width, with the theme's fill, rounded top corners and the frame line.
5. **Frame:** the top margin is the title bar's height alone; the frame line is drawn inside the bar at the top and as thin rectangles at the sides and bottom.
6. **Buttons:** each shows one of four states from the theme (normal, hover, pressed, window inactive) instead of the original's animated circle, drawn at the output's scale.
7. **Layout:** buttons are placed from the theme's sizes and gaps, at the side given by `button_side`, in the order given by `button_order`. The title can be left-aligned, right-aligned or centred, and is shortened with an ellipsis when it does not fit.
8. **Rim:** an optional hairline just outside the frame (`window.frame.rim`).
9. **Shadow:** drawn here from the theme's `[window.shadow]`, with separate values for active and inactive windows.
10. **Corners and edges:** optional rounded bottom corners, and a resize strip outside the window's edge for frames without a border.
11. **Decoration mode:** a window with an xdg-decoration object gets a frame by that object's state, the agreed mode or else the mode Wayfire is about to send (`wants_server_side`), not by the view's flag. Wayfire counts a window as server-side until the mode is agreed, and sets the flag only after the commit that agrees it, which also shows the window, so the original frames a window that asked to draw its own and its first configures are a title bar short; Discord shown again from the tray never answers them.
