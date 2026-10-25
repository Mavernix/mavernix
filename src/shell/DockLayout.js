// Layout arithmetic for a dock at the bottom of the screen. Every number
// comes from the theme's [dock] table. Positions along the dock are measured
// from the screen's left edge, across it upwards from the bottom edge.

.pragma library

// A theme size written as [factor, offset]: factor * tileSize + offset.
function sized(rule, tileSize) {
    return Math.trunc(rule[0] * tileSize + rule[1])
}

function iconSize(looks, config, screenHeight) {
    if (config.icon_size !== undefined) {
        return config.icon_size
    }
    if (looks.compact_icon_size !== undefined && screenHeight <= looks.compact_up_to_height) {
        return looks.compact_icon_size
    }
    return looks.icon_size
}

// Deliberately not rounded, so a shrunk row fills the width exactly.
function fittingTileSize(looks, wantedTileSize, units, screenWidth) {
    const spareTiles = looks.fit_spare_tiles
    const overhang = looks.fit_overhang
    if (wantedTileSize * (units + spareTiles) > screenWidth + overhang) {
        return (screenWidth + overhang) / (units + spareTiles)
    }
    return wantedTileSize
}

// Magnification: how far a point is pushed sideways, following a quarter
// sine wave around the pointer. Pushing points apart is what enlarges tiles.
function push(position, pointer, reach, amount) {
    if (amount === 0) {
        return 0
    }
    const distance = position - pointer
    if (distance <= -reach) {
        return -amount
    }
    if (distance >= reach) {
        return amount
    }
    return amount * Math.sin(Math.PI * distance / (2 * reach))
}

// The push amount that grows the tile under the pointer by `growth` pixels:
// each edge, half a tile away, moves by amount * sin(pi * tile / 4 reach).
function pushAmount(tileSize, reach, growth) {
    if (growth <= 0) {
        return 0
    }
    return growth / (2 * Math.sin(Math.PI * tileSize / (4 * reach)))
}

// The resting row (nothing magnified). `widths` are in tile units (1 for an
// icon, less for a separator). Returns { tiles: [{ left, width }], start, end }.
function row(looks, widths, tileSize, screenWidth) {
    let units = 0
    for (const width of widths) {
        units += width
    }
    const length = units * tileSize
    const start = screenWidth / 2 - length / 2 + looks.row_shift

    const tiles = []
    let position = start
    for (const width of widths) {
        tiles.push({ left: position, width: width * tileSize })
        position += width * tileSize
    }

    return { tiles: tiles, start: start, end: start + length }
}

// The current magnification, in the form pushedPosition() takes.
function pushNow(looks, tileSize, rowStart, rowEnd, pointer, growth) {
    const reach = looks.magnification_reach * tileSize
    return {
        pointer: Math.max(rowStart, Math.min(rowEnd, pointer)),
        reach: reach,
        amount: pushAmount(tileSize, reach, growth)
    }
}

function pushedPosition(position, now) {
    return position + push(position, now.pointer, now.reach, now.amount)
}

// Rounded so that neighbouring tiles still touch.
function tileEdges(restLeft, restWidth, now) {
    const left = pushedPosition(restLeft, now)
    const right = pushedPosition(restLeft + restWidth, now)
    const roundedLeft = Math.floor(left + 0.5)
    return { left: roundedLeft, right: roundedLeft + Math.floor(right - left + 0.5) }
}

function shelfEdges(looks, tileSize, rowStart, rowEnd, now) {
    const endPadding = sized(looks.end_padding, tileSize)
    const left = Math.floor(pushedPosition(rowStart, now) - endPadding + 0.5)
    const right = left + Math.floor(pushedPosition(rowEnd, now) + endPadding - looks.end_trim - left + 0.5)
    return { left: left, right: right }
}
