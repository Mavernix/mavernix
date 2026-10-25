// Makes a picture smaller by averaging blocks of pixels, so text and thin
// lines do not break up in a window's miniature.

#pragma once

#include <cstdint>
#include <vector>

namespace window_pictures
{
/** Rows of RGBA pixels. */
struct picture_t
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

/**
 * The given part of `source`, fitted into a square of `largest_side` pixels
 * (never enlarged). `source` is premultiplied, as the compositor keeps it;
 * the result is not, as PNG wants it.
 */
picture_t shrink_part(const picture_t& source, int left, int top, int part_width, int part_height, int largest_side);
} // namespace window_pictures
