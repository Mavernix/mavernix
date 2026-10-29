// How big a folder's grid in the dock gets, and how many columns and rows it
// shows. Every number comes from the theme's [dock.folder.grid] table
// (docs/themes/dock.md, "Folders in the dock").
//
// The grid is sized for a stage: the share `stage_share` of the free width
// and height. As many cells as the stage holds are shown at once, each
// counted with `fit_spacing` more room than it takes; the rest scroll. The
// shown cells make a near square, and shrink if the stage is too small for
// them at full size.

.pragma library

// Columns and rows for `count` cells: as many columns as its square root,
// and one more when the last row would otherwise hold no more cells than one
// fewer than there are rows (so 3 cells make 3 x 1, 7 make 4 x 2).
function nearSquare(count) {
    let columns = Math.ceil(Math.sqrt(count))
    let rows = Math.ceil(count / columns)
    const inLastRow = count % columns
    if (inLastRow !== 0 && inLastRow <= rows - 1) {
        columns += 1
        rows = Math.ceil(count / columns)
    }
    return { columns: columns, rows: rows }
}

// The grid for `entryCount` entries in a free area of freeWidth x
// freeHeight: { columns, shownRows, rows, cellScale }. `rows` counts every
// row, `shownRows` those in view at once; cells are drawn at cellScale
// (never above 1) of the theme's cell size.
function layout(looks, entryCount, freeWidth, freeHeight) {
    const cellWidth = looks.cell_width ?? 112
    const cellHeight = looks.cell_height ?? 112
    const sidePadding = looks.side_padding ?? 12
    const firstRowTop = looks.first_row_top ?? 36
    const bottomPadding = looks.bottom_padding ?? 8
    const stageShare = looks.stage_share ?? 1
    const fitSpacing = looks.fit_spacing ?? 0

    const stageWidth = Math.floor(Math.min(freeWidth * stageShare, freeWidth - 2 * sidePadding))
    const stageHeight = Math.floor(Math.min(freeHeight * stageShare, freeHeight - firstRowTop - bottomPadding))

    const cellsThatFit = Math.max(1, Math.floor(
        stageWidth / (cellWidth + fitSpacing) * (stageHeight / (cellHeight + fitSpacing))))
    const shown = nearSquare(Math.min(entryCount, cellsThatFit))

    // Cells shrink to fit the stage, in whole pixels, but never grow.
    const cellScale = Math.min(1, Math.floor(stageWidth / shown.columns) / cellWidth,
                               Math.floor(stageHeight / shown.rows) / cellHeight)
    return {
        columns: shown.columns,
        shownRows: shown.rows,
        rows: Math.ceil(entryCount / shown.columns),
        cellScale: cellScale,
    }
}
