# Sound packs

The desktop's sounds come from a **sound pack**: a folder of sound files, each named after the event it is played for. Packs are separate from themes: a theme names the pack that goes with it, and any other can be chosen instead, in the settings window's Sound pane or with `pack = "NAME"` under `[sounds]` in the desktop config.

## What a pack is

```
NAME/
    index.theme          three lines, see below
    stereo/
        trash-empty.wav
        message.wav
        ...
```

This is the layout Linux desktops use for sound themes (freedesktop.org's sound theme specification), so a sound theme installed on the system is a pack too, and a pack made here works in other desktops. Files may be `.wav`, `.oga` or `.ogg`; WAV is best, because the desktop keeps WAV sounds ready in memory and plays them without delay.

`index.theme`:

```
[Sound Theme]
Name=NAME
Directories=stereo

[stereo]
OutputProfile=stereo
```

## Where packs are looked for

By name, in `sounds/` under each of these, first found wins:

| Folder | For |
|---|---|
| `~/.local/share/sounds/` | your own packs |
| the desktop theme's `share/sounds/` | a pack that comes with a theme (the imported Sea Lion theme brings one) |
| `/usr/share/sounds/` | packs installed on the system |

## Making one

Make a folder `~/.local/share/sounds/MyPack/stereo/`, put sound files into it named after the events below, add the `index.theme`, and choose "MyPack" in the settings window. A pack need not have every event: an event without a file is silent.

A pack may also bring several alert sounds to choose among, as files named `alert-NAME.wav`; the settings window then offers them, and the one chosen is played for the alert events (`bell`, `dialog-warning`, `dialog-error`) instead of the pack's own files for those.

## The events

Names are those of freedesktop.org's sound naming specification where it has one.

| Event | Played when |
|---|---|
| `desktop-login` | the desktop has come up |
| `audio-volume-change` | the loudness was changed (keys, the wheel over the panel's volume item, mute) |
| `message` | a notification appears |
| `dialog-question` | the desktop asks before doing something (restart, emptying the trash) |
| `trash-empty` | the trash became empty |
| `file-trash` | something was moved to the trash |
| `dock-item-remove` | something was removed from the dock |
| `screen-capture` | a picture of the screen was taken (the screenshot keys) |
| `bell`, `dialog-warning`, `dialog-error` | a program beeps, warns or reports an error (raised by programs, not by the desktop itself yet) |

Anything can raise an event, also one of its own invention: `mavernix-sound EVENT` (a command in every session's path) plays the pack's sound for it, if it has one.

## The packs that come with the desktop

None: the plain desktop is silent. The Sea Lion theme brings a pack of its own, `SeaLion`, made by the Theme Installer; any pack installed on the system (`freedesktop`, for one) can be chosen in Settings, Sound.
