# Building and installing

**Mavernix 0.1.5 is made for Arch Linux**. Fedora 44 has also been tested in a KDE VM; source-build steps are below. Other distributions need compatible versions of the dependencies below. Quickshell and a current Wayfire are not packaged on many of them. The compositor plugins load only into the Wayfire version they were built against, so rebuild after Wayfire is updated.

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
| Adwaita icons, DejaVu fonts | | `adwaita-icon-theme` `adwaita-icon-theme-legacy` `ttf-dejavu` | the plain theme's icons and type (without the icons, the desktop falls back to Breeze, Papirus or hicolor) |
| foot (optional) | | `foot` | the terminal the dock and Super+Enter open; with another terminal, name it in the config instead |
| 7-Zip, cpio, bzip2 and lbzip2, libicns, FFmpeg, Poppler, bubblewrap | | `7zip` `cpio` `bzip2` `lbzip2` `libicns` `ffmpeg` `poppler` `bubblewrap` | the Theme Installer: reading the installer, converting icons, sounds and drawings, its sandbox |
| Meson, Ninja, a C++17 compiler, GLM, Vulkan headers | Meson 1.12.1 | `base-devel` `meson` `ninja` `glm` `vulkan-headers` | building (GLM and the Vulkan headers for Wayfire's own headers) |

On Arch, all of it at once:

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

Optional, each passed over if it is not installed:

| Package | For |
|---|---|
| `grim`, `slurp`, `wl-clipboard` | pictures of the screen |
| `brightnessctl`, `playerctl` | a laptop's brightness and media keys |
| `network-manager-applet` | choosing a wireless network |
| `dex` | starting the programs in `~/.config/autostart` |
| `hwdata` | the graphics card's name in About This Computer |
| `xdg-desktop-portal`, `xdg-desktop-portal-gtk`, `xdg-desktop-portal-wlr` | file choosers and screen sharing for browsers and sandboxed programs |

## On Arch Linux

The AUR has the latest release as [`mavernix`](https://aur.archlinux.org/packages/mavernix) (`yay -S mavernix`, or `paru`). Its `PKGBUILD` is `packaging/arch/PKGBUILD`, which also builds a release from a copy of this repository:

```sh
cd packaging/arch
makepkg -si
```

## On Fedora 44

For Fedora 44 x86_64. Builds Mavernix 0.1.5, Wayfire 0.11.0 and Quickshell 0.3.2 from pinned sources into `/opt/mavernix-source`. No RPM or COPR needed.

These versions and dependencies come from the working Fedora RPM build. This standalone `/opt` procedure has not been run end to end. Physical Wi-Fi/Bluetooth/audio and wallet unlock still need testing. Do not use it alongside the Mavernix RPM.

Run all commands in **Bash**, in the same terminal. From Fish, enter `bash` first.

### 1. Install dependencies

Build tools and headers:

```sh
sudo dnf install \
  git gcc-c++ meson ninja-build cmake \
  pkgconf-pkg-config python3 qt6-qtbase-private-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel \
  qt6-qtwayland-devel qt6-qtsvg-devel qt6-qtshadertools-devel glm-devel vulkan-headers \
  wayland-protocols-devel hwdata-devel scdoc glslang cli11-devel \
  'pkgconfig(tomlplusplus)' 'pkgconfig(librsvg-2.0)' 'pkgconfig(gio-unix-2.0)' 'pkgconfig(wayland-server)' 'pkgconfig(wayland-client)' \
  'pkgconfig(wayland-cursor)' 'pkgconfig(cairo)' 'pkgconfig(pango)' 'pkgconfig(pangocairo)' 'pkgconfig(libdrm)' \
  'pkgconfig(egl)' 'pkgconfig(glesv2)' 'pkgconfig(gbm)' 'pkgconfig(libinput)' 'pkgconfig(pixman-1)' \
  'pkgconfig(xkbcommon)' 'pkgconfig(libudev)' 'pkgconfig(libevdev)' 'pkgconfig(libxml-2.0)' 'pkgconfig(libseat)' \
  'pkgconfig(libdisplay-info)' 'pkgconfig(libliftoff)' 'pkgconfig(libjpeg)' 'pkgconfig(libpng)' 'pkgconfig(xcb)' \
  'pkgconfig(xcb-icccm)' 'pkgconfig(xcb-renderutil)' 'pkgconfig(xcb-errors)' 'pkgconfig(yyjson)' 'pkgconfig(jemalloc)' \
  'pkgconfig(libpipewire-0.3)' 'pkgconfig(pam)' 'pkgconfig(polkit-agent-1)' 'pkgconfig(xwayland)'
```

Desktop dependencies, including networking, Bluetooth, sound, screenshots, portals and theme importing:

```sh
sudo dnf install \
  python3 python3-pillow qt6-qtmultimedia qt6-qtwayland qt6-qtsvg \
  kvantum gvfs gsettings-desktop-schemas adwaita-icon-theme adwaita-icon-theme-legacy \
  adwaita-cursor-theme dejavu-sans-fonts foot xorg-x11-server-Xwayland NetworkManager \
  NetworkManager-wifi NetworkManager-bluetooth network-manager-applet nm-connection-editor bluez \
  pipewire pipewire-pulseaudio pipewire-alsa pipewire-utils wireplumber \
  upower polkit dex-autostart playerctl brightnessctl \
  grim slurp wl-clipboard xdg-desktop-portal xdg-desktop-portal-gtk \
  xdg-desktop-portal-wlr 7zip cpio bzip2 lbzip2 \
  libicns-utils ffmpeg-free poppler-utils bubblewrap
```

Keep Qt development and runtime packages at matching versions. Wayfire requires the newer libraries supplied by Fedora 44; these instructions are not for older Fedora releases.

### 2. Download sources

```sh
set -e
mkdir -p "$HOME/mavernix-source-build"
cd "$HOME/mavernix-source-build"
export MAVERNIX_SOURCE_ROOT="$PWD"
export MAVERNIX_SOURCE_PREFIX=/opt/mavernix-source

fetch_source() {
    git init "$1"
    git -C "$1" remote add origin "$2"
    git -C "$1" fetch --depth=1 origin "$3"
    git -C "$1" checkout --detach FETCH_HEAD
}
fetch_source mavernix https://github.com/Mavernix/mavernix.git 41682a3a8aba5f005fc745650250044e3c7796d4
fetch_source wayfire https://github.com/WayfireWM/wayfire.git d39902379c5737eebe1dc3a830659fa3f0fa22e0
fetch_source wayfire/subprojects/wf-config https://github.com/WayfireWM/wf-config.git 85cca14ac889b2f8bbd53569d9751ed2e8f5c079
fetch_source wayfire/subprojects/wlroots https://gitlab.freedesktop.org/wlroots/wlroots.git d783533489e1f75d6886c2ab5c5960090ef268f8
fetch_source wayfire/subprojects/wf-utils https://github.com/WayfireWM/wf-utils.git 329c3ff01724d82947f61c45332f75d3534e8454
fetch_source wayfire/subprojects/wf-touch https://github.com/WayfireWM/wf-touch.git d7ae5e7fa4de9536e2de2af74f168f981eb8a77a
fetch_source wayfire/subprojects/wf-json https://github.com/WayfireWM/wf-json.git 70039e13cdeaebd8ec498ed30bf5ab91c2e313ec
fetch_source quickshell https://git.outfoxxed.me/quickshell/quickshell.git 4f508be500dea6e5732cc3d50382a0048b17e7b1
```

Use an empty build folder. If you repeat the procedure, reuse the checked-out sources and skip this download step.

### 3. Build Wayfire

```sh
meson setup build-wayfire wayfire \
  --prefix="$MAVERNIX_SOURCE_PREFIX" --libdir=lib64 \
  --sysconfdir="$MAVERNIX_SOURCE_PREFIX/etc" \
  --buildtype=release --wrap-mode=nodownload \
  -Duse_system_wlroots=disabled -Duse_system_wfconfig=disabled \
  -Dvulkan_effects=false -Dbuild_locales=disabled -Dtests=disabled \
  -Dwf-config:tests=disabled -Dwlroots:renderers=gles2 \
  -Dwlroots:backends=drm,libinput,x11 -Dwlroots:xwayland=enabled
meson compile -C build-wayfire -j 2
sudo meson install -C build-wayfire --no-rebuild

export PATH="$MAVERNIX_SOURCE_PREFIX/bin:$PATH"
export PKG_CONFIG_PATH="$MAVERNIX_SOURCE_PREFIX/lib64/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$MAVERNIX_SOURCE_PREFIX/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
pkg-config --modversion wayfire
```

The last command must report at least `0.11.0`.

### 4. Build Quickshell

```sh
cmake -S quickshell -B build-quickshell -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$MAVERNIX_SOURCE_PREFIX" \
  -DCMAKE_INSTALL_LIBDIR=lib64 -DCMAKE_SKIP_INSTALL_RPATH=ON \
  -DCRASH_HANDLER=OFF -DHYPRLAND=OFF -DBUILD_TESTING=OFF \
  -DGIT_REVISION=4f508be500dea6e5732cc3d50382a0048b17e7b1 \
  -DDISTRIBUTOR=Fedora-source
cmake --build build-quickshell --parallel 2
sudo cmake --install build-quickshell
```

### 5. Build Mavernix

Apply Fedora's executable and wallet-helper paths to this source checkout:

```sh
python3 - <<'PYTHON'
from pathlib import Path
p = Path('mavernix/src/session/at_login.py')
s = p.read_text()
s = s.replace('"/usr/lib/pam_kwallet_init"', '"/usr/libexec/pam_kwallet_init"')
s = s.replace('"dex"', '"dex-autostart"')
p.write_text(s)
PYTHON

meson setup build-mavernix mavernix \
  --prefix="$MAVERNIX_SOURCE_PREFIX" --libdir=lib64 \
  --buildtype=release --wrap-mode=nodownload
meson compile -C build-mavernix -j 2
sudo meson install -C build-mavernix --no-rebuild
```

### 6. Add the login session

The wrapper sets the private runtime paths. Its autostart directory hides KDE's Video Bridge helper only in this session.

```sh
mkdir -p session-xdg/autostart
cat > session-xdg/autostart/org.kde.xwaylandvideobridge.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Xwayland Video Bridge
Hidden=true
EOF
sudo install -d /opt/mavernix-source/etc/xdg/autostart
sudo install -m 0644 session-xdg/autostart/org.kde.xwaylandvideobridge.desktop \
  /opt/mavernix-source/etc/xdg/autostart/

cat > mavernix-source-session <<'EOF'
#!/bin/bash
set -eu
export PATH="/opt/mavernix-source/bin:$PATH"
export LD_LIBRARY_PATH="/opt/mavernix-source/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_DATA_DIRS="/opt/mavernix-source/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export XDG_CONFIG_DIRS="/opt/mavernix-source/etc/xdg:${XDG_CONFIG_DIRS:-/etc/xdg}"
exec /opt/mavernix-source/lib64/mavernix/session/start-desktop "$@"
EOF
sudo install -m 0755 mavernix-source-session /usr/local/bin/mavernix-source-session

cat > mavernix-source.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Mavernix (source)
Comment=Mavernix built from source
Exec=/usr/local/bin/mavernix-source-session
DesktopNames=wayfire
EOF
sudo install -d /usr/share/wayland-sessions
sudo install -m 0644 mavernix-source.desktop /usr/share/wayland-sessions/
```

On a standard Fedora KDE installation, NetworkManager, UPower and the PipeWire user services should already be running. Enable Bluetooth if needed:

```sh
sudo systemctl enable --now bluetooth
```

Log out and select **Mavernix (source)** at the login screen. Wi-Fi and Bluetooth require an available adapter; a VM normally exposes virtual Ethernet rather than the host's wireless devices.

### Remove

Log into another desktop first. These commands remove only this source installation:

```sh
sudo rm -f /usr/share/wayland-sessions/mavernix-source.desktop
sudo rm -f /usr/local/bin/mavernix-source-session
sudo rm -rf /opt/mavernix-source
```

Installed Fedora dependencies remain available to other applications.

Source: [Mavernix](https://github.com/Mavernix/mavernix), pinned to `41682a3a8aba5f005fc745650250044e3c7796d4`.

## Build, test, install

If you followed the Fedora section, the build and installation are already complete. The commands below apply when compatible Wayfire and Quickshell installations are already available.

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
