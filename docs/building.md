# Building and installing

**Mavernix 0.1 is made for Arch Linux**, and that's the only place it's built and tested so far (October 2026). Other distributions can work if they have versions like the ones below, but none has been tested yet. Many of them also don't package Quickshell or a recent Wayfire.

One thing to remember: the compositor plugins only load into the exact Wayfire version they were built against. So whenever Wayfire is updated, rebuild Mavernix too.

## On Arch Linux

The easy way. The AUR has the latest release as [`mavernix`](https://aur.archlinux.org/packages/mavernix):

```sh
yay -S mavernix      # or paru
```

Its `PKGBUILD` lives in `packaging/arch/PKGBUILD`. You can also use it to build a release from your own copy of this repository:

```sh
cd packaging/arch
makepkg -si
```

## What you need

| What | Tested with | Arch package | What it's for |
|---|---|---|---|
| [Wayfire](https://github.com/WayfireWM/wayfire) | 0.11.0 (0.11 is required) | `wayfire` | the compositor |
| [Quickshell](https://quickshell.org) | 0.3.2 | `quickshell` | runs the shell's QML |
| Qt: Base, Declarative, Multimedia, Wayland, SVG | 6.12.0 | `qt6-base` `qt6-declarative` `qt6-multimedia` `qt6-wayland` `qt6-svg` | the shell, the Settings window, sounds, and icons that are drawings |
| [toml++](https://github.com/marzer/tomlplusplus) | 3.4.0 | `tomlplusplus` | reading themes in the compositor |
| GLib with GIO, and GVfs | 2.90.1, 1.62.0 | `glib2` `gvfs` | knowing which program opens which file, and the trash |
| librsvg | 2.63.4 | `librsvg` | icons that are drawings |
| Python, with Pillow | 3.14.8 (3.11 is the oldest that can work), 13.0.0 | `python` `python-pillow` | the session's scripts. Pillow cuts up the pictures for the toolkit themes. Without it, programs keep their own look. |
| Kvantum | 1.1.8 | `kvantum` | draws Qt programs in the theme's look |
| PipeWire and WirePlumber | | `pipewire` `wireplumber` | the desktop's sounds |
| GSettings schemas | 51.0 | `gsettings-desktop-schemas` | the look of GTK programs |
| Adwaita icons, DejaVu fonts | | `adwaita-icon-theme` `adwaita-icon-theme-legacy` `ttf-dejavu` | the plain theme's icons and fonts. Without the icons, the desktop falls back to Breeze, Papirus or hicolor. |
| foot (optional) | | `foot` | the terminal that the dock and Super+Enter open. Prefer another terminal? Name it in the config instead. |
| 7-Zip, cpio, bzip2 and lbzip2, libicns, FFmpeg, Poppler, bubblewrap | | `7zip` `cpio` `bzip2` `lbzip2` `libicns` `ffmpeg` `poppler` `bubblewrap` | the Theme Installer: reading the installer, converting icons, sounds and drawings, and its sandbox |
| Meson, Ninja, a C++17 compiler, GLM, Vulkan headers | Meson 1.12.1 | `base-devel` `meson` `ninja` `glm` `vulkan-headers` | building (Wayfire's own headers need GLM and the Vulkan headers) |

On Arch, you can grab all of it at once:

```sh
# to build
sudo pacman -S --needed base-devel meson ninja wayfire glm vulkan-headers tomlplusplus \
    qt6-base qt6-declarative qt6-multimedia qt6-wayland librsvg glib2

# to run
sudo pacman -S --needed quickshell python python-pillow qt6-svg kvantum \
    pipewire wireplumber gsettings-desktop-schemas gvfs \
    adwaita-icon-theme adwaita-icon-theme-legacy adwaita-cursors ttf-dejavu \
    7zip cpio bzip2 lbzip2 libicns ffmpeg poppler bubblewrap
```

These are optional. If one isn't installed, the desktop just does without it:

| Package | For |
|---|---|
| `grim`, `slurp`, `wl-clipboard` | screenshots |
| `brightnessctl`, `playerctl` | a laptop's brightness and media keys |
| `network-manager-applet` | asking for a network password NetworkManager needs again, and networks that sign in with a user name or a certificate |
| `dex` | starting the programs in `~/.config/autostart` |
| `hwdata` | the graphics card's name in About This Computer |
| `xdg-desktop-portal`, `xdg-desktop-portal-gtk`, `xdg-desktop-portal-wlr` | file pickers and screen sharing for browsers and sandboxed programs |

## Build, test, install

```sh
meson setup build
meson compile -C build
meson test -C build
sudo meson install -C build
```

By default this installs into `/usr/local`. Use `meson setup build --prefix=/usr` if you want `/usr` instead.

The login screen entry goes into `PREFIX/share/wayland-sessions/mavernix.desktop`. Login screens look in `/usr/share/wayland-sessions/`, so with the default prefix you'll need to link it there:

```sh
sudo ln -s /usr/local/share/wayland-sessions/mavernix.desktop /usr/share/wayland-sessions/
```

Then log out and pick "Mavernix" on the login screen.

To remove it again, run `sudo ninja -C build uninstall`.

## Try it without installing

`dev/run-in-container` builds and installs the desktop inside a throwaway [Podman](https://podman.io) container, then opens it in a VNC window (TigerVNC's `vncviewer`). Nothing on your machine changes.

```sh
dev/run-in-container              # the desktop a login would start
dev/run-in-container sea-lion     # another one, once you've imported its theme
dev/run-in-container stop
```

The container has no sound, network or battery services, so those panel items won't show up there.

## Try it in a window

Once it's installed, you can run the desktop as a window inside any Wayland desktop. It doesn't change anything outside the window, and it ends when you close the window:

```sh
WLR_BACKENDS=wayland /usr/local/lib/mavernix/session/start-desktop plain
```

Notifications and application menus then go to the outer desktop, if it handles them. The log is in `$XDG_RUNTIME_DIR/mavernix/session.log`.
