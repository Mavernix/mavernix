// The session's polkit authentication agent: asks for the user's password
// in the shell's ordinary dialog when the system needs it. The password goes
// straight back to polkit; nothing here keeps it. Wording is the config's
// [authentication].

import QtQuick
import Quickshell.Services.Polkit

Item {
    id: authentication

    // The settings object written by src/session/make_shell_config.py, and
    // the shell's ConfirmDialog.
    required property var settings
    required property var dialog

    readonly property var words: settings.config.authentication ?? ({})

    PolkitAgent {
        id: agent
    }
    readonly property var flow: agent.isActive ? agent.flow : null
    property bool asking: false

    // After a wrong password polkit asks again.
    readonly property bool wantsAnswer: flow !== null && flow.isResponseRequired
    onWantsAnswerChanged: {
        if (wantsAnswer) {
            ask()
        }
    }

    onFlowChanged: {
        if (flow === null && asking) {
            asking = false
            dialog.dismiss()
        }
    }

    function ask() {
        const request = flow
        asking = true
        dialog.ask({
            title: request.message,
            text: request.supplementaryMessage,
            icon: words.icon ?? "dialog-password",
            button: words.button ?? "OK",
            cancel: words.cancel ?? "Cancel",
            field: {
                placeholder: words.password_placeholder ?? "Password",
                secret: !request.responseVisible
            },
            agreed: typed => {
                authentication.asking = false
                request.submit(typed)
            },
            cancelled: () => {
                authentication.asking = false
                request.cancelAuthenticationRequest()
            }
        })
    }
}
