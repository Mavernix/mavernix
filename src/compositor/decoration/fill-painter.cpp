#include "fill-painter.hpp"

#include <cmath>
#include <map>
#include <string>

namespace themed
{
namespace
{
/** A loaded picture and its pixels per logical pixel (2 for an "@2x" file). */
struct picture_t
{
    cairo_surface_t *surface = nullptr;
    int pixels_per_logical_pixel = 1;
};

/** "close.png" becomes "close@2x.png". */
std::string double_density_name(const std::string& path)
{
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos)
    {
        return path + "@2x";
    }

    return path.substr(0, dot) + "@2x" + path.substr(dot);
}

cairo_surface_t *load_png(const std::string& path)
{
    cairo_surface_t *surface = cairo_image_surface_create_from_png(path.c_str());
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy(surface);
        return nullptr;
    }

    return surface;
}

/**
 * Pictures are loaded once and kept: a theme has a few dozen small ones.
 * A missing one is remembered too, so the disk is not asked on every repaint.
 */
const picture_t& picture_for(const std::string& path, double scale)
{
    static std::map<std::string, picture_t> loaded;

    // From 1.5 upwards the double-density picture looks better scaled down
    // than the ordinary one does scaled up.
    const bool want_double_density = (scale >= 1.5);
    const std::string cache_key    = path + (want_double_density ? "|2x" : "|1x");

    auto found = loaded.find(cache_key);
    if (found != loaded.end())
    {
        return found->second;
    }

    picture_t picture;
    if (want_double_density)
    {
        picture.surface = load_png(double_density_name(path));
        picture.pixels_per_logical_pixel = 2;
    }

    // No @2x file, or scale 1: the ordinary picture, enlarged if need be.
    if (picture.surface == nullptr)
    {
        picture.surface = load_png(path);
        picture.pixels_per_logical_pixel = 1;
    }

    return loaded[cache_key] = picture;
}

/** Copies one part of a picture into a destination rectangle, stretched. */
void paint_picture_part(cairo_t *cr, cairo_surface_t *picture,
    double source_x, double source_y, double source_width, double source_height,
    double x, double y, double width, double height)
{
    if ((source_width <= 0) || (source_height <= 0) || (width <= 0) || (height <= 0))
    {
        return;
    }

    cairo_save(cr);
    cairo_rectangle(cr, x, y, width, height);
    cairo_clip(cr);

    cairo_translate(cr, x, y);
    cairo_scale(cr, width / source_width, height / source_height);
    cairo_set_source_surface(cr, picture, -source_x, -source_y);

    // "Pad" repeats the outermost pixels beyond the picture's edge. Without
    // it, stretched edges fade to transparent along the seams between parts.
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);

    cairo_paint(cr);
    cairo_restore(cr);
}

/** Nine-slice: corners keep their size, edges stretch one way, the middle both. */
void paint_picture(cairo_t *cr, const fill_t& fill, double x, double y, double width,
    double height, double scale)
{
    const picture_t& picture = picture_for(fill.image_path, scale);
    if (picture.surface == nullptr)
    {
        // A loud colour, so that a missing picture is noticed.
        cairo_set_source_rgba(cr, 1.0, 0.0, 1.0, 1.0);
        cairo_rectangle(cr, x, y, width, height);
        cairo_fill(cr);
        return;
    }

    const double picture_width  = cairo_image_surface_get_width(picture.surface);
    const double picture_height = cairo_image_surface_get_height(picture.surface);

    const double in_picture = picture.pixels_per_logical_pixel;
    const double top    = fill.slice[0];
    const double right  = fill.slice[1];
    const double bottom = fill.slice[2];
    const double left   = fill.slice[3];

    // Column and row boundaries: {start, size} in the picture and on screen.
    const double source_columns[3][2] = {
        {0, left * in_picture},
        {left * in_picture, picture_width - (left + right) * in_picture},
        {picture_width - right * in_picture, right * in_picture},
    };
    const double target_columns[3][2] = {
        {x, left * scale},
        {x + left * scale, width - (left + right) * scale},
        {x + width - right * scale, right * scale},
    };
    const double source_rows[3][2] = {
        {0, top * in_picture},
        {top * in_picture, picture_height - (top + bottom) * in_picture},
        {picture_height - bottom * in_picture, bottom * in_picture},
    };
    const double target_rows[3][2] = {
        {y, top * scale},
        {y + top * scale, height - (top + bottom) * scale},
        {y + height - bottom * scale, bottom * scale},
    };

    for (int row = 0; row < 3; row++)
    {
        for (int column = 0; column < 3; column++)
        {
            paint_picture_part(cr, picture.surface,
                source_columns[column][0], source_rows[row][0],
                source_columns[column][1], source_rows[row][1],
                target_columns[column][0], target_rows[row][0],
                target_columns[column][1], target_rows[row][1]);
        }
    }
}
cairo_operator_t cairo_operator_for(overlay_t::blend_t blend)
{
    switch (blend)
    {
      case overlay_t::blend_t::OVERLAY:
        return CAIRO_OPERATOR_OVERLAY;

      case overlay_t::blend_t::MULTIPLY:
        return CAIRO_OPERATOR_MULTIPLY;

      case overlay_t::blend_t::SCREEN:
        return CAIRO_OPERATOR_SCREEN;

      default:
        return CAIRO_OPERATOR_OVER;
    }
}

/** Repeats a fill's texture over the rectangle from its top left corner. */
void paint_overlay(cairo_t *cr, const overlay_t& overlay, double x, double y, double width,
    double height, double scale)
{
    if (overlay.image_path.empty())
    {
        return;
    }

    const picture_t& picture = picture_for(overlay.image_path, scale);
    if (picture.surface == nullptr)
    {
        return;
    }

    cairo_save(cr);
    cairo_rectangle(cr, x, y, width, height);
    cairo_clip(cr);

    const double zoom = scale / picture.pixels_per_logical_pixel;
    cairo_translate(cr, x, y);
    cairo_scale(cr, zoom, zoom);
    cairo_set_source_surface(cr, picture.surface, 0, 0);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);

    cairo_set_operator(cr, cairo_operator_for(overlay.blend));
    cairo_paint_with_alpha(cr, overlay.opacity);
    cairo_restore(cr);
}
} // anonymous namespace

void set_source_color(cairo_t *cr, const rgba_t& color)
{
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
}

void rounded_rectangle_path(cairo_t *cr, double x, double y, double width, double height,
    const corner_radii_t& radii)
{
    // Cairo angles run clockwise from "pointing right".
    const double right_angle = M_PI / 2.0;

    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radii.top_right, y + radii.top_right,
        radii.top_right, -right_angle, 0);
    cairo_arc(cr, x + width - radii.bottom_right, y + height - radii.bottom_right,
        radii.bottom_right, 0, right_angle);
    cairo_arc(cr, x + radii.bottom_left, y + height - radii.bottom_left,
        radii.bottom_left, right_angle, 2 * right_angle);
    cairo_arc(cr, x + radii.top_left, y + radii.top_left,
        radii.top_left, 2 * right_angle, 3 * right_angle);
    cairo_close_path(cr);
}

void paint_fill(cairo_t *cr, const fill_t& fill, double x, double y, double width,
    double height, double scale)
{
    if (fill.kind == fill_t::kind_t::NONE)
    {
        return;
    }

    if (fill.kind == fill_t::kind_t::IMAGE)
    {
        paint_picture(cr, fill, x, y, width, height, scale);
        paint_overlay(cr, fill.overlay, x, y, width, height, scale);
        return;
    }

    const double radius = fill.radius * scale;
    const corner_radii_t all_corners = {radius, radius, radius, radius};

    cairo_save(cr);
    rounded_rectangle_path(cr, x, y, width, height, all_corners);

    if (fill.kind == fill_t::kind_t::COLOR)
    {
        set_source_color(cr, fill.color);
        cairo_fill(cr);
    } else // GRADIENT
    {
        cairo_pattern_t *gradient = cairo_pattern_create_linear(0, y, 0, y + height);
        for (const gradient_stop_t& stop : fill.gradient)
        {
            cairo_pattern_add_color_stop_rgba(gradient, stop.position,
                stop.color.red, stop.color.green, stop.color.blue, stop.color.alpha);
        }

        cairo_set_source(cr, gradient);
        cairo_fill(cr);
        cairo_pattern_destroy(gradient);
    }

    if (fill.border_width > 0)
    {
        // Cairo centres a line on its path, so move it in by half the width.
        const double line  = fill.border_width * scale;
        const double inset = line / 2.0;
        const double inner_radius = std::max(0.0, radius - inset);
        rounded_rectangle_path(cr, x + inset, y + inset, width - line, height - line,
            {inner_radius, inner_radius, inner_radius, inner_radius});
        set_source_color(cr, fill.border_color);
        cairo_set_line_width(cr, line);
        cairo_stroke(cr);
    }

    cairo_restore(cr);

    paint_overlay(cr, fill.overlay, x, y, width, height, scale);
}
}
