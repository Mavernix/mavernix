// Status item: the sound output's loudness. The wheel changes it; the menu
// has a slider, output and input devices, Mute and, if the config names
// a command, the sound settings.

import QtQuick
import Quickshell
import Quickshell.Services.Pipewire
import ".."

StatusItem {
    id: item

    // Shared with the volume keys; also plays the feedback sound.
    readonly property var volume: panel.outputVolume
    readonly property real loudness: volume.loudness
    readonly property bool muted: volume.muted

    shown: volume.available

    iconName: {
        if (muted || loudness <= 0) {
            return "audio-volume-muted"
        }
        if (loudness < 1 / 3) {
            return "audio-volume-low"
        }
        if (loudness < 2 / 3) {
            return "audio-volume-medium"
        }
        return "audio-volume-high"
    }

    // One wheel notch changes the loudness by [sounds] volume_step.
    readonly property real wheelStep:
        (panel.settings.config.sounds ?? ({})).volume_step ?? 0.05
    onScrolled: notches => volume.changeBy(notches * wheelStep)

    // Wording: [status.volume].
    readonly property var words: (panel.settings.config.status ?? ({})).volume ?? ({})

    // Sinks and sources, without per-program streams.
    readonly property var devices: (Pipewire.nodes?.values ?? []).filter(node => !node.isStream && node.audio !== null)
    readonly property var outputs: devices.filter(node => node.isSink)
    readonly property var inputs: devices.filter(node => !node.isSink)

    function nameOf(node) {
        return node.description !== "" ? node.description : (node.nickname !== "" ? node.nickname : node.name)
    }

    // Entry ids: fixed ones, and per-device ones counted from a base.
    readonly property int sliderId: 1
    readonly property int muteId: 2
    readonly property int settingsId: 3
    readonly property int outputBaseId: 1000
    readonly property int inputBaseId: 2000

    function entry(id, label, enabled, checked) {
        return { id: id, label: label, enabled: enabled, separator: false,
                 checkable: checked !== undefined, checked: checked ?? false, shortcut: [], children: [] }
    }
    readonly property var separator: ({ id: 0, label: "", enabled: false, separator: true,
                                        checkable: false, checked: false, shortcut: [], children: [] })

    // The menu is built from the values at opening, so it does not rebuild
    // while its slider is dragged.
    property real loudnessAtOpening: 0
    property bool mutedAtOpening: false
    onOpening: {
        loudnessAtOpening = loudness
        mutedAtOpening = muted
    }

    menuItems: {
        const slider = entry(sliderId, "", false)
        slider.slider = mutedAtOpening ? 0 : loudnessAtOpening
        const items = [slider, separator]

        if (outputs.length > 0) {
            items.push(entry(0, words.output_label ?? "Output Device:", false))
            for (let index = 0; index < outputs.length; index++) {
                items.push(entry(outputBaseId + index, nameOf(outputs[index]), true,
                                 outputs[index] === Pipewire.defaultAudioSink))
            }
        }
        if (inputs.length > 0) {
            items.push(separator)
            items.push(entry(0, words.input_label ?? "Input Device:", false))
            for (let index = 0; index < inputs.length; index++) {
                items.push(entry(inputBaseId + index, nameOf(inputs[index]), true,
                                 inputs[index] === Pipewire.defaultAudioSource))
            }
        }

        items.push(separator)
        items.push(entry(muteId, words.mute_label ?? "Mute", true, mutedAtOpening))
        if ((words.settings_command ?? []).length > 0) {
            items.push(separator)
            items.push(entry(settingsId, words.settings_label ?? "Sound Settings…", true))
        }
        return items
    }

    onSlid: (id, value) => volume.setTo(value)

    onChosen: id => {
        if (id === muteId) {
            volume.toggleMute()
        } else if (id === settingsId) {
            Quickshell.execDetached(words.settings_command)
        } else if (id >= inputBaseId) {
            Pipewire.preferredDefaultAudioSource = inputs[id - inputBaseId]
        } else if (id >= outputBaseId) {
            Pipewire.preferredDefaultAudioSink = outputs[id - outputBaseId]
        }
    }
}
