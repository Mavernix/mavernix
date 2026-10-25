#pragma once

/**
 * Draws a theme's fills (colour, gradient or stretched picture) with cairo,
 * for title bars and buttons alike. Positions and sizes are device pixels;
 * `scale` is device pixels per logical pixel.
 */

#include <cairo.h>

#include "theme-file.hpp"

namespace themed
{
/** In device pixels. */
struct corner_radii_t
{
    double top_left     = 0.0;
    double top_right    = 0.0;
    double bottom_right = 0.0;
    double bottom_left  = 0.0;
};

void rounded_rectangle_path(cairo_t *cr, double x, double y, double width, double height,
    const corner_radii_t& radii);

/**
 * The current cairo clip still applies, which is how the title bar gets its
 * rounded top corners whatever its fill is.
 */
void paint_fill(cairo_t *cr, const fill_t& fill, double x, double y, double width,
    double height, double scale);

void set_source_color(cairo_t *cr, const rgba_t& color);
}
