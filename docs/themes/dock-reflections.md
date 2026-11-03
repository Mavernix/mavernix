# Dock reflections

A desktop config can enable live window reflections on a slab shelf with `dock.window_reflections = true`. Sea Lion enables the effect for both appearances. Wayfire renders the visible window strip onto the glass at its original horizontal width. The renderer prefilters the strip before sampling a compressed reflection, so small details blend together as windows move.

The desktop config accepts `dock.window_reflection_blur` (0–12, in reflected logical pixels), `dock.window_reflection_opacity` (0–1) and `dock.window_reflection_compression` (0.05–1). The theme controls reflected icons separately with `dock.reflection.blur`, a fraction of the icon width. Set `dock.rear_outline = false` to omit the shelf’s rear rim.

The native renderer uses GLES. Both shelf surfaces follow the dock geometry and hide for an active fullscreen window on the dock screen.
