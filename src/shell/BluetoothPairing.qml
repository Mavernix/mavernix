// Questions a Bluetooth device asks while it is paired, put to the person
// in the shell's ordinary dialog: type a PIN, check that two codes match,
// or type a code on the device. Wording: [status.bluetooth].

import QtQuick
import Shell.Native

Item {
    id: pairing

    // The settings object written by src/session/make_shell_config.py, and
    // the shell's ConfirmDialog.
    required property var settings
    required property var dialog

    readonly property var words: (settings.config.status ?? ({})).bluetooth ?? ({})
    property bool asking: false

    BluetoothAgent {
        id: agent
        onAsked: (kind, device, code) => pairing.ask(kind, device, code)
        onCancelled: {
            if (pairing.asking) {
                pairing.asking = false
                pairing.dialog.dismiss()
            }
        }
    }

    function ask(kind, device, code) {
        const fill = text => text.replace("%1", device).replace("%2", code)
        const question = {
            icon: words.pairing_icon ?? "bluetooth",
            text: "",
            button: words.pair_label ?? "Pair",
            cancel: words.cancel_label ?? "Cancel",
            agreed: typed => {
                pairing.asking = false
                agent.answer(true, typed)
            },
            cancelled: () => {
                pairing.asking = false
                agent.answer(false, "")
            }
        }
        if (kind === "pin" || kind === "passkey") {
            question.title = fill(words.pin_label ?? "Type the PIN for “%1”.")
            question.text = words.pin_note ?? "It is often 0000 or 1234, or written in the device's manual."
            question.field = { placeholder: words.pin_placeholder ?? "PIN", secret: false }
        } else if (kind === "confirm") {
            question.title = fill(words.confirm_label ?? "Does “%1” show the code %2?")
            question.text = words.confirm_note ?? "Pair only if the same code is shown on both."
        } else if (kind === "show") {
            // Nothing to answer here: the code is typed on the device.
            question.title = fill(words.show_label ?? "Type %2 on “%1”, then press Return on it.")
            question.button = words.ok_label ?? "OK"
            question.cancel = ""
            question.agreed = () => { pairing.asking = false }
            question.cancelled = () => { pairing.asking = false }
        } else {
            question.title = fill(words.authorize_label ?? "Allow “%1” to connect to this computer?")
            question.button = words.allow_label ?? "Allow"
        }
        asking = true
        dialog.ask(question)
    }
}
