#!/usr/bin/env bash
# Runs inside the container started by dev/run-in-container: builds and
# installs the desktop, starts it on a virtual screen and serves it over VNC.
#
#   /src  the source tree, read-only
#   /out  dev/out/ on the host: logs, the session's state and the VNC socket
set -euo pipefail

desktop=${1:-}

# Build and install as root, then re-run as the host user's ID: Wayfire
# refuses to run as root, and /out must stay writable.
if [[ $(id -u) == 0 ]]; then
    if [[ -n $MAVERNIX_FEDORA ]]; then
        /src/packaging/fedora/build-rpms "/out/fedora-$MAVERNIX_FEDORA" > /out/build.log 2>&1
    else
        meson setup /build /src --prefix=/usr > /out/build.log 2>&1
        meson compile -C /build >> /out/build.log 2>&1
        meson install -C /build >> /out/build.log 2>&1
    fi

    mkdir -p /home/desktop
    chown "$DESKTOP_UID:$DESKTOP_GID" /home/desktop
    exec setpriv --reuid="$DESKTOP_UID" --regid="$DESKTOP_GID" --clear-groups \
        env HOME=/home/desktop "$0" "$desktop"
fi

# Wayland needs a private runtime folder.
export XDG_RUNTIME_DIR=/tmp/runtime
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

# The session bus at a fixed address, so `podman exec` programs find it.
export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
dbus-daemon --session --address="$DBUS_SESSION_BUS_ADDRESS" --fork

# Headless: no monitor or input devices.
export WLR_BACKENDS=headless
export WLR_LIBINPUT_NO_DEVICES=1
if [[ -e /dev/dri/renderD128 ]]; then
    export WLR_RENDER_DRM_DEVICE=/dev/dri/renderD128
else
    export WLR_RENDERER=pixman
fi

# Kvantum reads its theme choice only from the user's config folder.
mkdir -p "$HOME/.config/Kvantum"
ln -sfn /out/state/app-theme/share/themes/DesktopTheme/Kvantum "$HOME/.config/Kvantum/DesktopTheme"
printf '[General]\ntheme=DesktopTheme\n' > "$HOME/.config/Kvantum/kvantum.kvconfig"

# The virtual screen, the size of a 13-inch laptop's; with MAVERNIX_SCREENS=2,
# a 1920x1080 one to its right as well.
printf '[output:HEADLESS-1]\nmode = 1280x800\nscale = 1\n' > /out/screen.ini
if [[ ${MAVERNIX_SCREENS:-1} == 2 ]]; then
    export WLR_HEADLESS_OUTPUTS=2
    printf '[output:HEADLESS-2]\nmode = 1920x1080\nscale = 1\n' >> /out/screen.ini
    # The VNC window shows one screen, HEADLESS-1, so the menu bar and dock
    # start there (the user's choices, shared by the bundled desktops).
    mkdir -p "$HOME/.config/mavernix"
    printf '{"display": {"main_screen": "HEADLESS-1"}}\n' > "$HOME/.config/mavernix/plain.json"
fi

export DESKTOP_STATE_DIR=/out/state
export DESKTOP_EXTRA_INI=/out/screen.ini
# Fedora's packages keep it in /usr/lib64.
session_dir=/usr/lib/mavernix/session
if [[ -n $MAVERNIX_FEDORA ]]; then
    session_dir=/usr/lib64/mavernix/session
fi
"$session_dir/start-desktop" ${desktop:+"$desktop"} &
session=$!

# wayvnc is a Wayland client, so wait for the compositor's socket.
for _ in $(seq 50); do
    if compgen -G "$XDG_RUNTIME_DIR/wayland-*" > /dev/null; then
        break
    fi
    sleep 0.2
done
WAYLAND_DISPLAY=$(basename "$(compgen -G "$XDG_RUNTIME_DIR/wayland-[0-9]" | head -n1)")
export WAYLAND_DISPLAY
rm -f /out/vnc.sock
wayvnc --unix-socket /out/vnc.sock > /out/wayvnc.log 2>&1 &

wait "$session"
