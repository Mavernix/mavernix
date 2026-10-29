"""Does what has to happen once when a session has come up.

    python at_login.py SHELL-SETTINGS.json

Run by the compositor once it is up. Opens KDE's wallet with the login,
tells on-demand services (D-Bus, systemd) about the session's display and
settings (after-session takes them back later), starts the config's and the
user's autostart programs, places [desktop_files] place_once launchers once
per user, and lets the Theme Installer offer a new import of a look when this
version brings it something. Does nothing when nested in another desktop.
"""

import json
import os
import shutil
import subprocess
import sys

from paths import LIBRARY_DIR

ENVIRONMENT_FOR_SERVICES = ["WAYLAND_DISPLAY", "DISPLAY", "XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE"]

# Taken back out after the session (after-session). Without the look's names
# and XDG_DATA_DIRS (the theme's icons and pointers, the toolkit themes) a
# program started on demand, such as the file manager, gets the stock look.
# configure-desktop updates the look's names on a switch.
SESSION_SETTINGS_FOR_SERVICES = ["XDG_CONFIG_DIRS", "XDG_DATA_DIRS", "DCONF_PROFILE", "XCURSOR_PATH",
                                 "GTK_THEME", "QT_STYLE_OVERRIDE", "QT_QPA_SYSTEM_ICON_THEME",
                                 "XCURSOR_THEME", "XCURSOR_SIZE"]

# File managers that stay running as services between windows (Thunar's
# daemon, Nautilus): one started before the session keeps its look and finds
# none of the theme's icons. They come back on demand.
EARLY_FILE_MANAGER_SERVICES = ["thunar.service", "dbus-*-org.xfce.Thunar@*.service",
                               "dbus-*-org.gnome.Nautilus@*.service"]

# Sends the login to the wallet that KDE's PAM module started. Run here, not
# via dex, because programs started earlier would hit a still-closed wallet.
# Arch keeps it in /usr/lib, Fedora in /usr/libexec.
WALLET_LOGIN_HELPERS = ["/usr/lib/pam_kwallet_init", "/usr/libexec/pam_kwallet_init"]

# The autostart helper: dex on Arch, dex-autostart on Fedora.
DEX_NAMES = ["dex", "dex-autostart"]

# Says which imported looks a new import would bring something to.
THEME_IMPORT_BACKEND = LIBRARY_DIR / "theme-installer" / "import_theme.py"

PORTAL_SERVICE = "xdg-desktop-portal.service"
PORTAL_HELPERS = ["xdg-desktop-portal-gtk.service", "xdg-desktop-portal-wlr.service"]

def say(message):
    print(f"at_login: {message}", file=sys.stderr)


def hand_login_to_wallet():
    if not os.environ.get("PAM_KWALLET5_LOGIN"):
        return
    helper = next((path for path in WALLET_LOGIN_HELPERS if os.access(path, os.X_OK)), None)
    if helper is None:
        say("the login screen opened KDE's wallet socket, but pam_kwallet_init is missing; "
            "the wallet will ask for the password")
        return
    result = subprocess.run([helper], capture_output=True, text=True)
    if result.returncode != 0:
        say(f"could not hand the login to KDE's wallet: {result.stderr.strip()}")


def services_environment():
    """systemd's user environment as a dict; empty without a user manager."""
    result = subprocess.run(["systemctl", "--user", "show-environment"], capture_output=True, text=True)
    if result.returncode != 0:
        return {}
    environment = {}
    for line in result.stdout.splitlines():
        name, _, value = line.partition("=")
        environment[name] = value
    return environment


def arrange_taking_settings_back():
    """Starts after-session with the values the services had before."""
    before = services_environment()
    settings = []
    for name in SESSION_SETTINGS_FOR_SERVICES:
        # A value naming our state folder or toolkit theme is left over from
        # a crashed session.
        value = before.get(name)
        if value is None or "/mavernix/" in value or value == "DesktopTheme":
            settings.append(name)
        else:
            settings.append(f"{name}={value}")
    compositor = os.environ.get("DESKTOP_SESSION_PID")
    if not compositor:
        say("DESKTOP_SESSION_PID is not set; this session's settings will stay with the services after it")
        return False
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "after-session")
    result = subprocess.run(
        ["systemd-run", "--user", "--collect", "--quiet",
         "--unit", f"mavernix-after-{compositor}",
         "--description", "Takes the desktop session's settings back out after it",
         script, compositor] + settings,
        capture_output=True, text=True)
    if result.returncode != 0:
        say(f"could not arrange taking this session's settings back out: {result.stderr.strip()}")
        return False
    return True


def tell_services():
    names = [name for name in ENVIRONMENT_FOR_SERVICES if name in os.environ]
    if shutil.which("dbus-update-activation-environment") is None:
        say("dbus-update-activation-environment is not installed; services started on demand "
            "will not find this session's display")
        return
    command = ["dbus-update-activation-environment"]
    # The session's settings go out only where they can be taken back:
    # through systemd.
    runtime_directory = os.environ.get("XDG_RUNTIME_DIR", "")
    if os.path.exists(os.path.join(runtime_directory, "systemd", "private")):
        command.append("--systemd")
        if arrange_taking_settings_back():
            names += [name for name in SESSION_SETTINGS_FOR_SERVICES if name in os.environ]
    result = subprocess.run(command + names, capture_output=True, text=True)
    if result.returncode != 0:
        say(f"could not tell services about the session: {result.stderr.strip()}")
        return
    if "--systemd" in command:
        restart_early_portals()
        stop_early_kde_service()
        stop_early_file_managers()


def restart_early_portals():
    """Restarts portal services that came up before the environment was set.

    A portal activated early keeps an environment without WAYLAND_DISPLAY or
    XDG_CURRENT_DESKTOP, so it offers no ScreenCast, and the GTK helper hits
    systemd's start limit.
    """
    # try-restart touches only running services, starting nothing unasked.
    subprocess.run(["systemctl", "--user", "reset-failed"] + PORTAL_HELPERS,
                   capture_output=True)
    result = subprocess.run(["systemctl", "--user", "try-restart", PORTAL_SERVICE] + PORTAL_HELPERS,
                            capture_output=True, text=True)
    if result.returncode != 0:
        say(f"could not restart the portal services: {result.stderr.strip()}")


def stop_early_kde_service():
    """Stops a kded6 started before the environment was set, so it would
    miss the session's kded6rc. It is D-Bus activated and comes back on demand.
    """
    subprocess.run(["systemctl", "--user", "stop", "dbus-*-org.kde.kded6@*.service"],
                   capture_output=True)


def stop_early_file_managers():
    """Stops file managers kept running from before the session (see
    EARLY_FILE_MANAGER_SERVICES). At login none has a window of this session."""
    subprocess.run(["systemctl", "--user", "stop"] + EARLY_FILE_MANAGER_SERVICES, capture_output=True)


def start(command):
    """Starts a program on its own, not waiting for it."""
    subprocess.Popen(command, stdin=subprocess.DEVNULL, start_new_session=True)


def start_listed_programs(config):
    for command in config.get("session", {}).get("autostart", []):
        if shutil.which(command[0]) is None:
            # Not an error: not every machine has every listed program.
            say(f"not starting {command[0]}: it is not installed")
            continue
        start(command)


def start_users_programs():
    dex = next((name for name in DEX_NAMES if shutil.which(name)), None)
    if dex is None:
        say("dex is not installed, so the programs in ~/.config/autostart are not started "
            "(install dex; on Arch: sudo pacman -S --needed dex)")
        return
    # Honours OnlyShowIn/NotShowIn.
    desktop = os.environ.get("XDG_CURRENT_DESKTOP", "")
    start([dex, "--autostart", "--environment", desktop])


def placed_launchers_path():
    """Lists the launchers placed on the user's desktop so far."""
    config_home = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return os.path.join(config_home, "mavernix", "placed-on-desktop")


def place_launchers_once(config, settings_path):
    """Copies each [desktop_files] place_once entry to the desktop, once ever."""
    wished = config.get("desktop_files", {}).get("place_once", [])
    if not wished:
        return
    record = placed_launchers_path()
    try:
        with open(record) as file:
            placed = set(file.read().split())
    except FileNotFoundError:
        placed = set()

    desktop = os.path.expanduser(config.get("desktop_files", {}).get("folder", "~/Desktop"))
    # The session writes its programs' entries beside the settings file.
    entries = os.path.join(os.path.dirname(settings_path), "share", "applications")
    for program in wished:
        if program in placed:
            continue
        entry = os.path.join(entries, f"{program}.desktop")
        if not os.path.isfile(entry):
            say(f"no desktop entry for {program}, so no launcher is put on the desktop")
            continue
        name = program
        with open(entry) as file:
            for line in file:
                if line.startswith("Name="):
                    name = line[len("Name="):].strip()
                    break
        os.makedirs(desktop, exist_ok=True)
        launcher = os.path.join(desktop, f"{name}.desktop")
        shutil.copyfile(entry, launcher)
        # Executable, so the desktop treats it as a trusted launcher.
        os.chmod(launcher, 0o755)
        placed.add(program)
        say(f"put a launcher for {program} on the desktop")

    os.makedirs(os.path.dirname(record), exist_ok=True)
    with open(record, "w") as file:
        file.write("\n".join(sorted(placed)) + "\n")


def offer_new_theme_imports():
    """Opens the Theme Installer's list of what a new import would bring,
    when an imported look is older than this version and the user has not
    put that off. The Theme Installer itself shows it, so it can import."""
    if not THEME_IMPORT_BACKEND.is_file():
        return
    result = subprocess.run([sys.executable, str(THEME_IMPORT_BACKEND), "news"], capture_output=True, text=True)
    if result.returncode != 0:
        say(f"could not ask the Theme Installer what is new: {result.stderr.strip()}")
        return
    try:
        looks = json.loads(result.stdout)
    except ValueError:
        say(f"the Theme Installer's answer about what is new was not JSON: {result.stdout.strip()}")
        return
    if looks:
        start(["mavernix-theme-installer", "--whats-new"])


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    if os.environ.get("DESKTOP_NESTED"):
        say("this desktop runs inside another one; leaving services and login programs alone")
        return

    with open(sys.argv[1]) as file:
        config = json.load(file)["config"]

    place_launchers_once(config, sys.argv[1])

    hand_login_to_wallet()
    tell_services()
    start_listed_programs(config)
    if config.get("session", {}).get("users_autostart", True):
        start_users_programs()
    offer_new_theme_imports()


if __name__ == "__main__":
    main()
