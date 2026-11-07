// A notice of the shell's own, shown as a banner like a program's
// notification (Banners.showOwn): it has the properties of one that the
// banners and the notification list read. `pressed` runs when it is clicked.

import QtQuick

QtObject {
    // Below zero, so it never meets a program's notification's id.
    property int id: -1
    property string appName: ""
    property string appIcon: ""
    property string image: ""
    property string desktopEntry: ""
    property string summary: ""
    property string body: ""
    // 0: the banner time chosen in Settings.
    property int expireTimeout: 0
    property bool tracked: true
    readonly property bool fromShell: true

    property var pressed: null
    readonly property var actions: [{ identifier: "default", invoke: () => pressed?.() }]

    signal closed()
    function dismiss() {
        closed()
    }
}
