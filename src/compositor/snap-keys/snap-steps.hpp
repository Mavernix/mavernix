// What a press of an arrow key does to a window, depending on where it is
// now. Free of Wayfire so tests/test-snap-keys.cpp can check the table.

#pragma once

#include <cstdint>

namespace snap_keys
{
// Tiled edges as a bit mask, equal to wlroots' WLR_EDGE_* (checked at
// compile time in the plugin). A half has three edges, a quarter two, the
// whole screen four; none means floating.
const uint32_t EDGE_TOP = 1;
const uint32_t EDGE_BOTTOM = 2;
const uint32_t EDGE_LEFT = 4;
const uint32_t EDGE_RIGHT = 8;
const uint32_t EDGES_ALL = EDGE_TOP | EDGE_BOTTOM | EDGE_LEFT | EDGE_RIGHT;

enum class snap_direction
{
    LEFT,
    RIGHT,
    UP,
    DOWN,
};

enum class snap_kind
{
    // Nothing to do; the key is taken all the same.
    NOTHING,
    TILE,
    // Back to the size and place it had before it was tiled.
    RESTORE,
    MINIMIZE,
};

struct snap_step
{
    snap_kind kind;
    // For TILE: the edges to tile against.
    uint32_t edges;
};

// Any `current_edges` that is not a half, a quarter or the whole screen is
// treated like a floating window.
snap_step next_snap_step(uint32_t current_edges, snap_direction direction);
} // namespace snap_keys
