# Profiling

`dev/run-in-container` draws everything on the CPU (the container has no GPU
driver of its own), so it's good for checking behaviour but its timings say
little about a real machine. Measure on real hardware, in a session of its own
that leaves the desktop you're working in alone.

## A session of its own

Install a build into a folder of its own:

```sh
meson setup /tmp/bench-build . --prefix=/tmp/bench-prefix
meson compile -C /tmp/bench-build && meson install -C /tmp/bench-build
```

Start it headless on the GPU, with its own home, runtime folder and session
bus. An empty `XDG_CONFIG_DIRS` keeps autostart programs (nm-applet,
kdeconnectd, …) from starting a second time. The runtime folder must have a
short path: a Wayland socket's path can't be longer than 108 bytes.

```sh
rt=/run/user/$(id -u)/mavernix-bench
home=/tmp/bench-home
mkdir -p "$home/.config" "$home/xdg-empty" "$rt" && chmod 700 "$rt"
printf '[output:HEADLESS-1]\nmode = 1280x800\nscale = 1\n' > "$home/screen.ini"
env -i PATH=/usr/bin HOME="$home" USER="$USER" XDG_RUNTIME_DIR="$rt" \
    XDG_CONFIG_HOME="$home/.config" XDG_CACHE_HOME="$home/.cache" \
    XDG_CONFIG_DIRS="$home/xdg-empty" XDG_DATA_HOME=/path/to/a/copy/of/.local/share \
    WLR_BACKENDS=headless WLR_LIBINPUT_NO_DEVICES=1 \
    WLR_RENDER_DRM_DEVICE=/dev/dri/renderD128 \
    DESKTOP_STATE_DIR="$home/state" DESKTOP_EXTRA_INI="$home/screen.ini" \
    DESKTOP_EXTRA_PLUGINS=stipc \
    dbus-run-session -- /tmp/bench-prefix/lib/mavernix/session/start-desktop sea-lion
```

`XDG_DATA_HOME` should point at a copy of your `~/.local/share` (themes,
desktops), not the real one. Fix up the theme's font symlink in the copy.

To stop it, end only the processes whose environment names that runtime
folder (`grep -z mavernix-bench /proc/PID/environ`), then unmount its
`gvfs` and `doc` FUSE mounts with `fusermount3 -u` before removing the folder.
Never `pkill` a program by name: that also hits the one on your own desktop.

## Driving it

- Shell actions: `XDG_RUNTIME_DIR=$rt qs -p /tmp/bench-prefix/share/mavernix/shell ipc call shell run app_grid`
  (also `notification_list`, `window_overview`, …).
- Pointer and keys: the `stipc` plugin, through the compositor's socket
  (`$rt/wayfire-*.socket`, 4-byte little-endian length, then JSON):
  `stipc/move_cursor {x, y}`, `stipc/feed_button {combo: "BTN_LEFT", mode: "press"|"release"}`,
  `stipc/feed_key {key: "KEY_ESC", state: true|false}`, `stipc/run {cmd}`.
- Screenshots: `grim` with the same `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY=wayland-1`.
  An idle headless screen makes `grim` wait until something redraws; move the
  pointer first.

Always look at a screenshot once before trusting a scripted measurement: a
press a few pixels off the dock's divider measures nothing at all.

## What to measure

- **Latency**: grab screenshots in a loop while acting, hash a region, and
  take the time from the press to the first changed frame and to the last.
- **CPU**: `utime + stime` from `/proc/PID/stat` before and after. Total CPU
  can mislead: with the NVIDIA driver the render thread busy-waits, and work
  taken off the main thread shows up there instead. Split it by thread.
- **Where the time goes**: `perf`. Without root it needs
  `sudo sysctl kernel.perf_event_paranoid=1` (until the next boot).

  ```sh
  perf record -F 999 -g -p "$(pgrep -f '^qs -p /tmp/bench-prefix')" -- sleep 3
  perf report --no-children --sort comm -n --stdio -g none   # main thread is "qs"
  perf report --children --sort sym --stdio -g none          # the hot functions
  ```

- **Frames**: `QSG_RENDER_TIMING=1 QT_LOGGING_RULES=qt.scenegraph.time.renderloop=true`
  in the environment makes Qt log every frame's sync, render and swap time to
  the session's `login.log`.
- **Memory**: `RssAnon` in `/proc/PID/status`, read 30 s or more after the
  shell started (it is about 180 MB higher for the first 20 s). For what holds
  it, `heaptrack` under `run-shell`; end the shell with SIGTERM so the trace
  is written.

Run each comparison at least twice, before and after, in the same session
setup; single runs vary by tens of milliseconds.

## Known numbers (RTX 3060 Ti, 1280×800, 83 programs)

| | |
|---|---|
| Shell, settled | about 315 MB anonymous memory |
| Menu open and close | +22 ms compositor, +15 ms shell CPU |
| Dock folder grid (100 files) | first frame 50–90 ms, fully open 130–210 ms |
| App grid | first frame about 30 ms; 130–200 ms shell CPU per open |
| System Preferences pane change | about 75 ms compositor and 95 ms app CPU: the heavy one |
