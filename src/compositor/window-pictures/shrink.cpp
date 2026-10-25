#include "shrink.hpp"

#include <algorithm>

namespace window_pictures
{
const int BYTES_PER_PIXEL = 4;
const int OPACITY_BYTE = 3;
const int FULL = 255;

picture_t shrink_part(const picture_t& source, int left, int top, int part_width, int part_height, int largest_side)
{
    picture_t result;
    const int longer_side = std::max(part_width, part_height);
    if ((longer_side <= 0) || (largest_side <= 0))
    {
        return result;
    }

    // Never enlarged: a window smaller than the square keeps its size.
    const int divisor = std::max(longer_side, largest_side);
    result.width = std::max(1, part_width * largest_side / divisor);
    result.height = std::max(1, part_height * largest_side / divisor);
    result.pixels.resize(size_t(result.width) * result.height * BYTES_PER_PIXEL);

    for (int row = 0; row < result.height; row++)
    {
        // The block of source rows this row stands for; at least one row.
        const int first_row = top + row * part_height / result.height;
        const int end_row = std::max(first_row + 1, top + (row + 1) * part_height / result.height);

        for (int column = 0; column < result.width; column++)
        {
            const int first_column = left + column * part_width / result.width;
            const int end_column = std::max(first_column + 1, left + (column + 1) * part_width / result.width);

            uint64_t sums[BYTES_PER_PIXEL] = {0, 0, 0, 0};
            for (int y = first_row; y < end_row; y++)
            {
                const uint8_t* pixel = &source.pixels[(size_t(y) * source.width + first_column) * BYTES_PER_PIXEL];
                for (int x = first_column; x < end_column; x++)
                {
                    for (int byte = 0; byte < BYTES_PER_PIXEL; byte++)
                    {
                        sums[byte] += pixel[byte];
                    }

                    pixel += BYTES_PER_PIXEL;
                }
            }

            const uint64_t count = uint64_t(end_row - first_row) * (end_column - first_column);
            const uint64_t opacity = sums[OPACITY_BYTE] / count;
            uint8_t* out = &result.pixels[(size_t(row) * result.width + column) * BYTES_PER_PIXEL];
            for (int byte = 0; byte < OPACITY_BYTE; byte++)
            {
                // The sums are of colours multiplied by opacity; dividing
                // by the summed opacity gives the plain colour back.
                const uint64_t plain = sums[OPACITY_BYTE] > 0 ? sums[byte] * FULL / sums[OPACITY_BYTE] : 0;
                out[byte] = uint8_t(std::min<uint64_t>(plain, FULL));
            }

            out[OPACITY_BYTE] = uint8_t(opacity);
        }
    }

    return result;
}
} // namespace window_pictures
