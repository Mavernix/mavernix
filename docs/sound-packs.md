# Sound packs

The desktop's sounds come from a **sound pack**: a folder of sound files, each one named after the event it plays for.

Packs and themes are separate things. A theme names the pack that goes with it, but you can pick any other pack instead. Do that in the Sound pane in Settings, or with `pack = "NAME"` under `[sounds]` in the desktop config.

## What's in a pack

```
NAME/
    index.theme          three short sections, see below
    stereo/
        trash-empty.wav
        message.wav
        ...
```

This is the same layout other Linux desktops use for sound themes (freedesktop.org's sound theme specification). That means any sound theme installed on your system already works as a pack, and a pack you make here works on other desktops too.

Files can be `.wav`, `.oga` or `.ogg`. WAV is best: the desktop keeps WAV sounds ready in memory, so they play without any delay.

`index.theme` looks like this:

```
[Sound Theme]
Name=NAME
Directories=stereo

[stereo]
OutputProfile=stereo
```

## Where packs are found

Packs are looked up by name in the `sounds/` folder under each of these, in order. The first match wins.

| Folder | For |
|---|---|
| `~/.local/share/sounds/` | your own packs |
| the desktop theme's `share/sounds/` | a pack that comes with a theme (the imported Sea Lion theme brings one) |
| `/usr/share/sounds/` | packs installed on the system |

## Making your own

1. Make a folder `~/.local/share/sounds/MyPack/stereo/`.
2. Put sound files into it, named after the events below.
3. Add the `index.theme`.
4. Pick "MyPack" in Settings.

That's it :) A pack doesn't need a file for every event. An event without one just stays silent.

A pack can also come with several alert sounds to choose from, as files named `alert-NAME.wav`. Settings then offers them, and the one you pick plays for the alert events (`bell`, `dialog-warning`, `dialog-error`) instead of the pack's own files for those.

## The events

Where freedesktop.org's sound naming specification has a name for an event, that's the name used here.

| Event | Plays when |
|---|---|
| `desktop-login` | the desktop has started |
| `audio-volume-change` | the volume changed (with the keys, the scroll wheel over the panel's volume item, or mute) |
| `message` | a notification appears |
| `dialog-question` | the desktop asks before doing something (restarting, emptying the trash) |
| `trash-empty` | the trash was emptied |
| `file-trash` | something was moved to the trash |
| `dock-item-remove` | something was removed from the dock |
| `screen-capture` | a screenshot was taken (with the screenshot keys) |
| `bell`, `dialog-warning`, `dialog-error` | a program beeps, warns or reports an error (only programs raise these, the desktop itself doesn't yet) |

Anything can play an event, even one you made up: `mavernix-sound EVENT` plays the pack's sound for it, if the pack has one. The command is on the path in every session.

## Packs that come with the desktop

None. The plain desktop is silent. The Sea Lion theme brings its own pack, `SeaLion`, which the Theme Installer makes for you. You can also pick any pack installed on your system (`freedesktop`, for example) in Settings, Sound.
