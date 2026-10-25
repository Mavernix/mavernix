// Where windows go in the window overview. Each program's windows keep
// their arrangement and are scaled as a group; groups stand in centred
// rows. All windows share one scale: the largest at which everything
// fits, capped by the theme.

.pragma library

const MOST_GROUPS_IN_A_ROW = 3

function boundsOf(windows) {
    let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
    for (const window of windows) {
        left = Math.min(left, window.x)
        top = Math.min(top, window.y)
        right = Math.max(right, window.x + window.width)
        bottom = Math.max(bottom, window.y + window.height)
    }
    return { x: left, y: top, width: right - left, height: bottom - top }
}

// One group per program, in order of first appearance: [{ appId, windows, bounds }].
function grouped(windows) {
    const groups = []
    for (const window of windows) {
        let group = groups.find(other => other.appId === window.appId)
        if (group === undefined) {
            group = { appId: window.appId, windows: [] }
            groups.push(group)
        }
        group.windows.push(window)
    }
    for (const group of groups) {
        group.bounds = boundsOf(group.windows)
    }
    return groups
}

function inRows(groups, rowCount) {
    const perRow = Math.ceil(groups.length / rowCount)
    const rows = []
    for (let first = 0; first < groups.length; first += perRow) {
        rows.push(groups.slice(first, first + perRow))
    }
    return rows
}

function scaleThatFits(rows, area, gap) {
    let widest = 0
    let heightOfAll = 0
    let mostInARow = 0
    for (const row of rows) {
        widest = Math.max(widest, row.reduce((sum, group) => sum + group.bounds.width, 0))
        heightOfAll += Math.max(...row.map(group => group.bounds.height))
        mostInARow = Math.max(mostInARow, row.length)
    }
    const acrossScale = (area.width - gap * (mostInARow - 1)) / widest
    const downScale = (area.height - gap * (rows.length - 1)) / heightOfAll
    return Math.min(acrossScale, downScale)
}

// `windows` are { id, appId, x, y, width, height } in screen points.
// Returns { scale, places, groups }: `places[id]` is a window's rectangle in
// the overview, `groups` each group's [{ appId, x, y, width, height }].
function place(windows, area, mostScale, gap) {
    const groups = grouped(windows)
    if (groups.length === 0) {
        return { scale: mostScale, places: {}, groups: [] }
    }

    // Try each row count and keep the one that lets the windows be largest.
    let rows = [groups]
    let scale = 0
    const mostRows = Math.ceil(groups.length / MOST_GROUPS_IN_A_ROW) + 1
    for (let rowCount = 1; rowCount <= Math.min(mostRows, groups.length); rowCount++) {
        const candidate = inRows(groups, rowCount)
        const fits = scaleThatFits(candidate, area, gap)
        if (fits > scale) {
            rows = candidate
            scale = fits
        }
    }
    scale = Math.min(scale, mostScale)

    const rowHeights = rows.map(row => Math.max(...row.map(group => group.bounds.height)) * scale)
    const heightOfAll = rowHeights.reduce((sum, height) => sum + height, 0) + gap * (rows.length - 1)

    const places = {}
    const placedGroups = []
    let rowTop = area.y + (area.height - heightOfAll) / 2
    rows.forEach((row, rowNumber) => {
        const rowWidth = row.reduce((sum, group) => sum + group.bounds.width * scale, 0)
            + gap * (row.length - 1)
        let groupLeft = area.x + (area.width - rowWidth) / 2
        for (const group of row) {
            const width = group.bounds.width * scale
            const height = group.bounds.height * scale
            // Groups stand on their row's bottom line, so their names are level.
            const groupTop = rowTop + rowHeights[rowNumber] - height
            for (const window of group.windows) {
                places[window.id] = {
                    x: groupLeft + (window.x - group.bounds.x) * scale,
                    y: groupTop + (window.y - group.bounds.y) * scale,
                    width: window.width * scale,
                    height: window.height * scale
                }
            }
            placedGroups.push({ appId: group.appId, x: groupLeft, y: groupTop, width: width, height: height })
            groupLeft += width + gap
        }
        rowTop += rowHeights[rowNumber] + gap
    })

    return { scale: scale, places: places, groups: placedGroups }
}
