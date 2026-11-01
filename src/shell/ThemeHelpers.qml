// ThemeValues.js for extensions: `import qs` brings the shell's QML types but
// not its script files, so this passes the helpers on.
//
// Usage:  import qs
//         color: ThemeHelpers.color(looks.text_color)

pragma Singleton

import QtQuick
import Quickshell
import "ThemeValues.js" as ThemeValues

Singleton {
    function color(text) {
        return ThemeValues.color(text)
    }

    function gradientPositions(fill) {
        return ThemeValues.gradientPositions(fill)
    }

    function easing(curve) {
        return ThemeValues.easing(curve)
    }
}
