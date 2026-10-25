"""Builds the Sea Lion theme's sounds from the installer's sound files.

    python make_sounds.py WORK OUTPUT/sea-lion

WORK holds the unpacked installer under root/; run it after make_theme.py.
It writes a freedesktop.org sound theme (THEME/share/sounds/SeaLion/), with
each AIFF file rewritten as WAV by ffmpeg and named after its event, in the
sound naming specification's names where it has one.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

SOUND_THEME_NAME = "SeaLion"

SYSTEM_SOUNDS = "System/Library/Components/CoreAudio.component/Contents/SharedSupport/SystemSounds"

# Event name -> the installer's file, from the unpacked system's root.
EVENT_SOUNDS = {
    "audio-volume-change": "System/Library/LoginPlugins/BezelServices.loginPlugin/Contents/Resources/volume.aiff",
    "trash-empty": f"{SYSTEM_SOUNDS}/finder/empty trash.aif",
    "screen-capture": f"{SYSTEM_SOUNDS}/system/Grab.aif",
    "camera-shutter": f"{SYSTEM_SOUNDS}/system/Grab.aif",
    "device-added": f"{SYSTEM_SOUNDS}/system/Volume Mount.aif",
    "device-removed": f"{SYSTEM_SOUNDS}/system/Volume Unmount.aif",
    # Events the naming specification has no name for; the names are ours.
    "file-trash": f"{SYSTEM_SOUNDS}/finder/move to trash.aif",
    "dock-item-trash": f"{SYSTEM_SOUNDS}/dock/drag to trash.aif",
    "dock-item-remove": f"{SYSTEM_SOUNDS}/dock/poof item off dock.aif",
}

# The alert sounds a user can choose among, kept as "alert-NAME".
ALERT_SOUNDS_DIRECTORY = "System/Library/Sounds"

# The alert sound when none was chosen.
DEFAULT_ALERT = "Funk"
# The events that are alerts: a program's beep, and its warnings and errors.
ALERT_EVENTS = ["bell", "dialog-warning", "dialog-error"]


def fail(message):
    sys.exit(f"make_sounds: {message}")


def convert(source, target):
    """Rewrites an AIFF file as WAV with the same samples."""
    if not source.is_file():
        fail(f"{source} is missing; has the installer image been unpacked?")
    result = subprocess.run(
        ["ffmpeg", "-loglevel", "error", "-y", "-i", str(source), "-c:a", "pcm_s16le", str(target)],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        fail(f"ffmpeg could not convert {source}: {result.stderr.strip()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("work", type=Path, help="the work folder, holding the unpacked installer under root/")
    parser.add_argument("theme", type=Path, help="the theme folder to write into")
    arguments = parser.parse_args()

    if shutil.which("ffmpeg") is None:
        fail("ffmpeg is not installed; it is what rewrites the installer's sound files. Install ffmpeg and run this again.")

    root = arguments.work / "root"
    sound_theme = arguments.theme / "share" / "sounds" / SOUND_THEME_NAME
    if sound_theme.exists():
        shutil.rmtree(sound_theme)
    sounds_directory = sound_theme / "stereo"
    sounds_directory.mkdir(parents=True)

    for event, source_path in EVENT_SOUNDS.items():
        convert(root / source_path, sounds_directory / f"{event}.wav")

    alerts = sorted((root / ALERT_SOUNDS_DIRECTORY).glob("*.aiff"))
    if not alerts:
        fail(f"no alert sounds in {root / ALERT_SOUNDS_DIRECTORY}")
    for alert in alerts:
        convert(alert, sounds_directory / f"alert-{alert.stem}.wav")
    for event in ALERT_EVENTS:
        shutil.copyfile(sounds_directory / f"alert-{DEFAULT_ALERT}.wav",
                        sounds_directory / f"{event}.wav")

    # Sounds this theme lacks are looked for in the stock theme.
    (sound_theme / "index.theme").write_text(
        "[Sound Theme]\n"
        f"Name={SOUND_THEME_NAME}\n"
        "Inherits=freedesktop\n"
        "Directories=stereo\n"
        "\n"
        "[stereo]\n"
        "OutputProfile=stereo\n"
    )
    print(f"wrote {len(EVENT_SOUNDS) + len(ALERT_EVENTS)} event sounds and {len(alerts)} alert sounds "
          f"into {sound_theme}")


if __name__ == "__main__":
    main()
