// Helpers for turning theme file values into what QML wants.

.pragma library
.import QtQuick as QtQuick

// Turns "#rrggbb" or "#rrggbbaa" into a QML colour. QML reads eight-digit
// strings as "#aarrggbb", so passing them through would swap channels.
function color(text) {
    if (text === undefined || text === null) {
        return "transparent"
    }

    const red = parseInt(text.substr(1, 2), 16) / 255
    const green = parseInt(text.substr(3, 2), 16) / 255
    const blue = parseInt(text.substr(5, 2), 16) / 255
    const alpha = text.length === 9 ? parseInt(text.substr(7, 2), 16) / 255 : 1.0
    return Qt.rgba(red, green, blue, alpha)
}

// A gradient's colour positions, 0.0 (top) to 1.0 (bottom); spread evenly
// unless the theme gives them.
function gradientPositions(fill) {
    if (fill.positions !== undefined) {
        return fill.positions
    }

    const count = fill.gradient.length
    const positions = []
    for (let index = 0; index < count; index++) {
        positions.push(index / (count - 1))
    }
    return positions
}

// Maps "linear", "ease-in", "ease-out", "ease-in-out" to a QML easing type;
// anything else gets "ease-in-out".
function easing(curve) {
    if (curve === "linear") {
        return QtQuick.Easing.Linear
    }
    if (curve === "ease-in") {
        return QtQuick.Easing.InQuad
    }
    if (curve === "ease-out") {
        return QtQuick.Easing.OutQuad
    }
    return QtQuick.Easing.InOutQuad
}
