# Where this code comes from

**Wayfire's animate plugin does the work around the effects.** It notices a window being minimised or restored, keeps the window on screen until the effect is over, and can run an effect backwards. Other plugins may add effects to it; this one adds `genie` and `scale`. Nothing of the animate plugin is copied.

**`warp-transformer` started from the animate plugin's "squeezimize" effect.**

| | |
|---|---|
| Origin | [Wayfire](https://github.com/WayfireWM/wayfire), `plugins/animate/squeezimize.hpp` |
| Version | 0.11.0, tag `v0.11.0` |
| Licence | MIT, copyright Scott Moreau. The text is in `LICENSE-wayfire` and stays with this code |

## Changes from the original

1. The window's picture is drawn as a mesh of triangles whose shape is computed in `warp-geometry`, instead of being bent in a fragment shader.
2. The shapes are different: a genie funnel and a straight scale, in place of squeezimize's curve.
3. One transformer serves both effects.
4. The picture is fetched at the output's density.
5. Only an icon below the window gets the genie shape; any other arrangement falls back to the scale shape.

## Known limits

- Works only with Wayfire's GLES renderer; with the Vulkan renderer the window is not drawn during the effect.
