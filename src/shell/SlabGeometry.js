// The outline of a "slab" shelf: a flat board seen from above and in front.
// Pure geometry for the slab's left end; SlabShelf.qml mirrors it for the
// right end.
//
// Each part is a "chain" running up the left end: a start point, then nodes
// reached by a straight line or, with two control points, a cubic curve:
//     [ { end }, { end }, { control1, control2, end }, ... ]
// Coordinates have x to the right and y upwards from the bottom edge.

.pragma library

// Control point distance, as a share of the radius, for a cubic curve that
// follows a quarter circle: 4/3 * (sqrt(2) - 1).
const QUARTER_CIRCLE = 0.552285

function point(x, y) {
    return { x: x, y: y }
}

function between(from, to, share) {
    return point(from.x + (to.x - from.x) * share, from.y + (to.y - from.y) * share)
}

// Splits a cubic curve at `share` (de Casteljau).
// Returns { first: [4 points], second: [4 points] }.
function splitCurve(start, control1, control2, end, share) {
    const a = between(start, control1, share)
    const b = between(control1, control2, share)
    const c = between(control2, end, share)
    const ab = between(a, b, share)
    const bc = between(b, c, share)
    const middle = between(ab, bc, share)
    return {
        first: [start, a, ab, middle],
        second: [middle, bc, c, end]
    }
}

// The share along a cubic curve where x turns (dx/ds = 0, a quadratic), or
// -1 if that is not strictly inside the curve.
function shareWhereXTurns(x0, x1, x2, x3) {
    // x'(s) / 3 = a s^2 + b s + c
    const a = -x0 + 3 * x1 - 3 * x2 + x3
    const b = 2 * (x0 - 2 * x1 + x2)
    const c = x1 - x0

    if (a === 0) {
        if (b === 0) {
            return -1
        }
        return -c / b
    }

    const underRoot = b * b - 4 * a * c
    if (underRoot < 0) {
        return -1
    }
    const candidates = [(-b + Math.sqrt(underRoot)) / (2 * a), (-b - Math.sqrt(underRoot)) / (2 * a)]
    for (const share of candidates) {
        if (share > 0 && share < 1) {
            return share
        }
    }
    return -1
}

// The front corner's size, interpolated linearly from the theme's two
// heights.
function frontCornerSize(looks, height) {
    const heights = looks.front_corner_heights
    const sizes = looks.front_corner_sizes
    const share = (height - heights[0]) / (heights[1] - heights[0])
    return sizes[0] + (sizes[1] - sizes[0]) * share
}

// Every point of the left end of a slab `height` high. The end leans out by
// `slant` per unit down; the front corner is a quarter ellipse, as wide as
// the corner size and `slant` times as high, sheared by the same lean.
function leftEnd(looks, height) {
    const slant = looks.slant
    const faceHeight = looks.face_height
    const lineHeight = looks.line_height
    const backRadius = looks.back_corner_radius
    const corner = frontCornerSize(looks, height)

    const frontY = faceHeight
    const backY = height - looks.clear_above

    // The front corner, first placed with the leaning line through x = 0 at
    // the front edge.
    const cornerStart = point(corner, frontY)
    const cornerControl1 = point(corner * (1 - QUARTER_CIRCLE), frontY)
    const cornerControl2 = point(corner * slant * slant * QUARTER_CIRCLE, frontY + corner * slant * QUARTER_CIRCLE)
    const cornerEnd = point(corner * slant * slant, frontY + corner * slant)

    // The corner bulges further left than its ends; shift everything so that
    // point sits `clear_beside` from the left edge.
    const turnShare = shareWhereXTurns(cornerStart.x, cornerControl1.x, cornerControl2.x, cornerEnd.x)
    const halves = splitCurve(cornerStart, cornerControl1, cornerControl2, cornerEnd, turnShare)
    const leftmost = halves.first[3]
    const shift = looks.clear_beside - leftmost.x

    function moved(p) {
        return point(p.x + shift, p.y)
    }

    function leanX(y) {
        return shift + slant * (y - frontY)
    }

    // The back corner: a quarter circle leaving the leaning line half a radius
    // below the back edge and meeting the edge one radius further in.
    const backCornerFromY = backY - backRadius / 2
    const backCornerControlOffset = backRadius * (1 - QUARTER_CIRCLE) / 2
    const backCorner = {
        start: point(leanX(backCornerFromY), backCornerFromY),
        control1: point(leanX(backY - backCornerControlOffset), backY - backCornerControlOffset),
        control2: point(leanX(backY) + backRadius * (1 - QUARTER_CIRCLE), backY),
        end: point(leanX(backY) + backRadius, backY)
    }

    // The bright line follows the corner's lower half, squeezed so that it ends
    // `line_height` above the front edge and vanishes at the leftmost point.
    const leftmostRise = leftmost.y - frontY
    function raisedForLine(p) {
        const rise = p.y - frontY
        const squeeze = (leftmostRise - lineHeight) / leftmostRise
        return point(p.x + shift, frontY + leftmostRise + (rise - leftmostRise) * squeeze)
    }

    function lowered(p) {
        return point(p.x, p.y - faceHeight)
    }

    function lineTo(end) {
        return { end: end }
    }

    function curveTo(control1, control2, end) {
        return { control1: control1, control2: control2, end: end }
    }

    // The front corner split at its leftmost point.
    const lower = halves.first.map(moved)
    const upper = halves.second.map(moved)
    const lowerRaised = halves.first.map(raisedForLine)
    const lowerDropped = lower.map(lowered)

    const surface = [
        lineTo(lower[0]),
        curveTo(lower[1], lower[2], lower[3]),
        curveTo(upper[1], upper[2], upper[3]),
        lineTo(backCorner.start),
        curveTo(backCorner.control1, backCorner.control2, backCorner.end)
    ]

    // The front face: the corner's lower half moved down, then back along the
    // corner itself.
    const face = [
        lineTo(lowerDropped[0]),
        curveTo(lowerDropped[1], lowerDropped[2], lowerDropped[3]),
        lineTo(lower[3]),
        curveTo(lower[2], lower[1], lower[0])
    ]

    const line = [
        lineTo(lower[0]),
        curveTo(lower[1], lower[2], lower[3]),
        curveTo(lowerRaised[2], lowerRaised[1], lowerRaised[0])
    ]

    const silhouette = [
        lineTo(lowerDropped[0]),
        curveTo(lowerDropped[1], lowerDropped[2], lowerDropped[3]),
        lineTo(lower[3]),
        curveTo(upper[1], upper[2], upper[3]),
        lineTo(backCorner.start),
        curveTo(backCorner.control1, backCorner.control2, backCorner.end)
    ]

    return {
        surface: surface,
        face: face,
        line: line,
        silhouette: silhouette,
        // The slab is at least twice this wide.
        backEdgeStartX: backCorner.end.x
    }
}
