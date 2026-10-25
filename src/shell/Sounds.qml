// Plays the desktop's sounds by event name: sounds.play("trash-empty").
//
// Sounds come from a sound pack (docs/sound-packs.md), chosen by the user,
// else [sounds] pack, else the theme. WAV files are kept in memory
// (SoundEffect) so they can follow a key press at once; other formats go
// through a MediaPlayer.

import QtQuick
import QtMultimedia
import Shell.Native

Item {
    id: sounds

    required property var settings
    required property var choices

    readonly property bool enabled: choices.value("sounds", "enabled") ?? true
    readonly property string packName:
        choices.value("sounds", "pack") ?? (settings.theme.sounds ?? ({})).pack ?? ""

    SoundPack {
        id: pack
        name: sounds.packName
        onSoundsChanged: {
            if (sounds.packName !== "" && folder === "") {
                console.warn("sounds: no sound pack named", sounds.packName,
                             "- a pack is a folder share/sounds/NAME/stereo/ among the data folders",
                             "(~/.local/share, the theme's, the project's, /usr/share)")
            }
        }
    }

    // Players are remade whenever the default output changes: players created
    // at login, before PipeWire has found the sound card, stay silent forever.
    MediaDevices {
        id: outputs
    }
    readonly property string outputName: String(outputs.defaultAudioOutput.id)

    // The output name, once it has stayed the same for a moment. Remaking all
    // players while PipeWire tears down a Bluetooth device has crashed the
    // shell; waiting for it to settle is meant to avoid that (not proven).
    property string settledOutputName: outputName
    readonly property int outputSettleMs: 1000
    onOutputNameChanged: settleTimer.restart()
    Timer {
        id: settleTimer
        interval: sounds.outputSettleMs
        onTriggered: sounds.settledOutputName = sounds.outputName
    }

    // Reading settledOutputName remakes both lists, and their players, when
    // the output changes.
    readonly property var readyEvents: {
        settledOutputName
        return Object.keys(pack.sounds).filter(event => pack.sounds[event].endsWith(".wav"))
    }
    readonly property var streamedEvents: {
        settledOutputName
        return Object.keys(pack.sounds).filter(event => !pack.sounds[event].endsWith(".wav"))
    }

    // A pack may bring several "alert-NAME" files; the user's choice among them
    // replaces these events' sounds.
    readonly property var alertEvents: ["bell", "dialog-warning", "dialog-error"]
    readonly property string chosenAlert: choices.value("sounds", "alert") ?? ""

    function play(event) {
        if (!enabled) {
            return
        }
        if (chosenAlert !== "" && alertEvents.includes(event)
                && pack.sounds["alert-" + chosenAlert] !== undefined) {
            event = "alert-" + chosenAlert
        }
        const ready = readyEvents.indexOf(event)
        if (ready >= 0) {
            readyPlayers.objectAt(ready).play()
            return
        }
        const streamed = streamedEvents.indexOf(event)
        if (streamed >= 0) {
            const player = streamedPlayers.objectAt(streamed)
            player.stop()
            player.play()
        }
    }

    Instantiator {
        id: readyPlayers
        model: sounds.readyEvents
        delegate: SoundEffect {
            required property string modelData
            source: "file://" + pack.sounds[modelData]
            audioDevice: outputs.defaultAudioOutput
            onStatusChanged: {
                if (status === SoundEffect.Error) {
                    console.warn("sounds: cannot play", source, "on the sound output", sounds.outputName,
                                 "- either the file is no WAV file, or there is no usable output yet")
                }
            }
        }
    }

    Instantiator {
        id: streamedPlayers
        model: sounds.streamedEvents
        delegate: MediaPlayer {
            required property string modelData
            source: "file://" + pack.sounds[modelData]
            audioOutput: AudioOutput {
                device: outputs.defaultAudioOutput
            }
            onErrorOccurred: (error, text) =>
                console.warn("sounds: cannot play", source, "-", text)
        }
    }
}
