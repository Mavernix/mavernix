// The loudness of the default sound output, to read and to change. A change
// made here plays the theme's "audio-volume-change" sound.

import QtQuick
import Quickshell.Services.Pipewire

Item {
    id: volume

    // The shell's Sounds, for the sound after a change; may be null.
    property var sounds: null
    property bool playsFeedback: true

    readonly property var output: Pipewire.defaultAudioSink
    readonly property var sound: output !== null ? output.audio : null
    readonly property bool available: sound !== null
    // From 0 to 1.
    readonly property real loudness: sound !== null ? sound.volume : 0
    readonly property bool muted: sound !== null ? sound.muted : false

    // Pipewire only reports an object's values while it is tracked.
    PwObjectTracker {
        objects: volume.output !== null ? [volume.output] : []
    }

    // The loudness was changed through this object, not by another program.
    signal changedHere()

    function feedback() {
        if (sounds !== null && playsFeedback) {
            sounds.play("audio-volume-change")
        }
        changedHere()
    }

    // Changes the loudness by a share of the full range; also ends a mute.
    function changeBy(share) {
        if (sound === null) {
            return
        }
        sound.muted = false
        sound.volume = Math.max(0, Math.min(1, loudness + share))
        feedback()
    }

    // Sets the loudness without the feedback sound (for a slider being dragged).
    function setTo(share) {
        if (sound === null) {
            return
        }
        sound.muted = false
        sound.volume = Math.max(0, Math.min(1, share))
    }

    function toggleMute() {
        if (sound === null) {
            return
        }
        sound.muted = !muted
        feedback()
    }
}
