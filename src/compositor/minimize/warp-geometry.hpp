// The shapes a window takes on its way into (or out of) its icon: genie
// (through a funnel) and scale (straight). Pure arithmetic; a shape is a list
// of rows from the window's top to its bottom, drawn by warp-transformer.

#pragma once

#include <vector>

namespace warp
{
/** Logical pixels, y downwards. */
struct box_t
{
    double x, y, width, height;
};

/** One horizontal cut through the warped window. */
struct row_t
{
    double y;
    double left_x;
    double right_x;
    // Which part of the picture it shows: 0 top edge, 1 bottom edge.
    double picture_height;
};

/** Slow, fast, slow; 0..1 in and out. */
double ease(double share);

/**
 * The genie shape at `progress` (0: the window as it is, 1: inside the icon)
 * for an icon below the window. An empty list means nothing is visible.
 */
std::vector<row_t> genie_rows(const box_t& window, const box_t& icon, double progress,
    double row_step);

/** The scale shape at `progress`: two rows, top and bottom. */
std::vector<row_t> scale_rows(const box_t& window, const box_t& icon, double progress);
}
