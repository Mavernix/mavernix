// Paints a "slab" shelf: a flat board seen from above and in front, with a
// lit top surface, a front face and a bright line between them. Shapes come
// from SlabGeometry.js, sizes and colours from the theme.

import QtQuick
import "SlabGeometry.js" as SlabGeometry
import "ThemeValues.js" as ThemeValues

Canvas {
    id: shelf

    required property var looks

    // No devicePixelRatio scaling here: the canvas already paints at the
    // screen's resolution, and scaling again doubles the shelf's size.
    onLooksChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    // The geometry has y upwards from the bottom edge; the canvas counts y
    // downwards. These map a point at the left end and its mirror at the right.
    function atLeft(p) {
        return { x: p.x, y: height - p.y }
    }

    function atRight(p) {
        return { x: width - p.x, y: height - p.y }
    }

    // Adds a closed outline: up the left end, across, and mirrored back down.
    function addMirroredOutline(context, chain) {
        const first = atLeft(chain[0].end)
        context.moveTo(first.x, first.y)

        for (let index = 1; index < chain.length; index++) {
            addStep(context, chain[index].control1, chain[index].control2, chain[index].end, atLeft)
        }

        const acrossTheBack = atRight(chain[chain.length - 1].end)
        context.lineTo(acrossTheBack.x, acrossTheBack.y)

        // Walking a step backwards swaps a curve's control points.
        for (let index = chain.length - 1; index >= 1; index--) {
            addStep(context, chain[index].control2, chain[index].control1, chain[index - 1].end, atRight)
        }

        context.closePath()
    }

    function addStep(context, control1, control2, end, place) {
        const target = place(end)
        if (control1 === undefined) {
            context.lineTo(target.x, target.y)
            return
        }
        const first = place(control1)
        const second = place(control2)
        context.bezierCurveTo(first.x, first.y, second.x, second.y, target.x, target.y)
    }

    // The theme lists the colours from the front (bottom) to the back.
    function frontToBackGradient(context, colors) {
        const gradient = context.createLinearGradient(0, height, 0, 0)
        for (let index = 0; index < colors.length; index++) {
            gradient.addColorStop(index / (colors.length - 1), ThemeValues.color(colors[index]))
        }
        return gradient
    }

    onPaint: {
        const context = getContext("2d")
        context.reset()

        const end = SlabGeometry.leftEnd(looks, height)

        // The rim: stroke at twice its width, then cut away the inner half.
        if (looks.outline !== undefined) {
            context.beginPath()
            addMirroredOutline(context, end.silhouette)
            context.strokeStyle = ThemeValues.color(looks.outline)
            context.lineWidth = 2 * looks.outline_width
            context.stroke()

            context.globalCompositeOperation = "destination-out"
            context.fillStyle = "black"
            context.fill()
            context.globalCompositeOperation = "source-over"
        }

        context.beginPath()
        addMirroredOutline(context, end.surface)
        context.fillStyle = frontToBackGradient(context, looks.surface)
        context.fill()

        context.beginPath()
        addMirroredOutline(context, end.face)
        context.fillStyle = frontToBackGradient(context, looks.face)
        context.fill()

        context.beginPath()
        addMirroredOutline(context, end.line)
        context.fillStyle = ThemeValues.color(looks.line)
        context.fill()
    }
}
