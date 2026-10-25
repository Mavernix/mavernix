// What happens to tiled windows when the line between them is dragged.
// Free of Wayfire's types so tests/test-shared-edges.cpp can check it.

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace shared_edges
{
/** A window's frame, in pixels. */
struct box_t
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct window_t
{
    box_t box;
    // Frame included; a maximum of 0 means no limit.
    int min_width = 1;
    int min_height = 1;
    int max_width = 0;
    int max_height = 0;
};

enum class side_t
{
    LEFT,
    RIGHT,
    TOP,
    BOTTOM,
};

/** The windows that move together when one seam is dragged. */
struct seam_t
{
    // A vertical seam has windows side by side and moves sideways.
    bool vertical = true;
    // Indices of windows whose right/bottom edge (before) or left/top edge
    // (after) is on the seam.
    std::vector<std::size_t> before;
    std::vector<std::size_t> after;
};

/**
 * The seam at `side` of windows[dragged], with every window that moves with
 * it, or nothing if no other window touches that edge. Windows on the same
 * line but separated from the dragged one by a gap stay out.
 */
std::optional<seam_t> find_seam(
    const std::vector<window_t>& windows, std::size_t dragged, side_t side, int tolerance_px);

/**
 * How far the seam may move for a pointer move of `wanted_px` (negative: left
 * or up), within every window's size limits and `floor_px`.
 */
int limit_move(const std::vector<window_t>& windows, const seam_t& seam, int wanted_px, int floor_px);

/** Where a window lies once the seam has been moved by `move_px`. */
box_t moved_box(const window_t& window, const seam_t& seam, std::size_t index, int move_px);
} // namespace shared_edges
