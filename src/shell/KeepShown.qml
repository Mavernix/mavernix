// Puts a window up again after its screen went away and came back, as a
// monitor does across sleep or when it is unplugged. The compositor closes
// the window with its screen; Quickshell then counts it as hidden, and
// neither moving it to the screen that comes back nor a `visible` binding
// that stays true shows it again. So the window's `visible` also asks this:
//
//     visible: open && keepShown.canShow

import QtQuick

Connections {
    // False from the window's closing until it is on a screen again.
    property bool canShow: true

    function onClosed() {
        canShow = false
    }

    function onScreenChanged() {
        // Without a name it is Qt's stand-in while no screen is there, which
        // the compositor has nowhere to put the window on.
        if (!canShow && (target.screen?.name ?? "") !== "") {
            canShow = true
        }
    }
}
