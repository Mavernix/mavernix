# Building and installing

**Mavernix 0.1 is made for Arch Linux**, and built and tested there only (October 2026). Other distributions can work if they have versions like those below, but none is tested yet, and Quickshell and a current Wayfire are not packaged on many of them. The compositor plugins load only into the Wayfire version they were built against, so rebuild after Wayfire is updated.

## What is needed

| What | Tested with | Arch package | Why |
|---|---|---|---|
| [Wayfire](https://github.com/WayfireWM/wayfire) | 0.11.0 (0.11 is required) | `wayfire` | the compositor |
| [Quickshell](https://quickshell.org) | 0.3.2 | `quickshell` | runs the shell's QML |
| Qt: Base, Declarative, Multimedia, Wayland, SVG | 6.12.0 | `qt6-base` `qt6-declarative` `qt6-multimedia` `qt6-wayland` `qt6-svg` | the shell, the settings window, sounds, icons that are drawings |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | `tomlplusplus` | reads themes in the compositor |
| GLib with GIO, and GVfs | 2.90.1, 1.62.0 | `glib2` `gvfs` | which program opens which file; the wastebasket |
| librsvg | 2.63.4 | `librsvg` | icons that are drawings |
| Python, with Pillow | 3.14.8 (3.11 is the oldest that can work), 13.0.0 | `python` `python-pillow` | the session's scripts; Pillow cuts the pictures for the toolkit themes (without it applications keep their own look) |
| Kvantum | 1.1.8 | `kvantum` | draws Qt applications in the theme's look |
| PipeWire and WirePlumber | | `pipewire` `wireplumber` | the desktop's sounds |
| GSettings schemas | 51.0 | `gsettings-desktop-schemas` | the look of GTK applications |
| Meson, Ninja, a C++17 compiler, GLM, Vulkan headers | Meson 1.12.1 | `base-devel` `meson` `ninja` `glm` `vulkan-headers` | building (GLM and the Vulkan headers for Wayfire's own headers) |

On Arch, all of it at once:

```sh
# to build
sudo pacman -S --needed base-devel meson ninja wayfire glm vulkan-headers tomlplusplus \
    qt6-base qt6-declarative qt6-multimedia qt6-wayland librsvg glib2

# to run
sudo pacman -S --needed quickshell python python-pillow qt6-svg kvantum \
    pipewire wireplumber gsettings-desktop-schemas gvfs
```

Optional, each passed over if it is not installed:

| Package | For |
|---|---|
| `foot` | the terminal Super+Enter opens |
| `grim`, `slurp`, `wl-clipboard` | pictures of the screen |
| `brightnessctl`, `playerctl` | a laptop's brightness and media keys |
| `network-manager-applet` | choosing a wireless network |
| `dex` | starting the programs in `~/.config/autostart` |
| `xdg-desktop-portal`, `xdg-desktop-portal-gtk`, `xdg-desktop-portal-wlr` | file choosers and screen sharing for browsers and sandboxed programs |
| `7zip`, `cpio`, `bzip2`, `libicns`, `ffmpeg`, `poppler`, `bubblewrap`, `lbzip2` | the Theme Installer (see [using the desktop](usage.md#the-sea-lion-look)) |

## On Arch Linux

`packaging/arch/PKGBUILD` builds a release into a package:

```sh
cd packaging/arch
makepkg -si
```

## Build, test, install

```sh
meson setup build
meson compile -C build
meson test -C build
sudo meson install -C build
```

This installs into `/usr/local` by default; `meson setup build --prefix=/usr` installs into `/usr`. The login screen's entry goes into `PREFIX/share/wayland-sessions/mavernix.desktop`; login screens look in `/usr/share/wayland-sessions/`, so with the default prefix, link it there:

```sh
sudo ln -s /usr/local/share/wayland-sessions/mavernix.desktop /usr/share/wayland-sessions/
```

Then log out and choose "Mavernix" at the login screen.

To remove it again: `sudo ninja -C build uninstall`.

## Try it without installing

`dev/run-in-container` builds and installs the desktop inside a throwaway [Podman](https://podman.io) container and opens it in a VNC window (TigerVNC's `vncviewer`). Nothing on your machine changes.

```sh
dev/run-in-container            # the desktop a login would start
dev/run-in-container sea-lion     # another one, once its theme is imported
dev/run-in-container stop
```

The container has no sound, network or battery services, so those panel items are missing there.

## Try it in a window

Once installed, the desktop can run as a window inside any Wayland desktop. It changes nothing outside the window and ends when the window is closed:

```sh
WLR_BACKENDS=wayland /usr/local/lib/mavernix/session/start-desktop plain
```

Notifications and application menus then go to the outer desktop if it handles them. Its log is `$XDG_RUNTIME_DIR/mavernix/session.log`.
