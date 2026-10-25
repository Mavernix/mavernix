#include "deco-theme.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include <wayfire/core.hpp>
#include <wayfire/debug.hpp>

#include "fill-painter.hpp"

namespace wf
{
namespace decor
{
namespace
{
/** Every window has its own decoration_theme_t; the parsed theme is shared. */
std::shared_ptr<const themed::window_theme_t> shared_theme(const std::string& directory)
{
    static std::map<std::string, std::shared_ptr<const themed::window_theme_t>> loaded;

    auto found = loaded.find(directory);
    if (found != loaded.end())
    {
        return found->second;
    }

    std::shared_ptr<const themed::window_theme_t> theme;
    try
    {
        theme = std::make_shared<themed::window_theme_t>(themed::load_window_theme(directory));
        LOGI("themed-decoration: loaded theme from ", directory);
    } catch (const std::exception& error)
    {
        LOGE("themed-decoration: cannot use the theme in \"", directory, "\": ", error.what(),
            ". Using the built-in plain look instead. Set the option "
            "themed-decoration/theme to a theme directory.");
        theme = std::make_shared<themed::window_theme_t>(themed::fallback_window_theme());
    }

    // A failure is remembered too, so the error is logged only once.
    return loaded[directory] = theme;
}

const themed::button_theme_t& button_theme(const themed::window_theme_t& theme,
    button_type_t button)
{
    switch (button)
    {
      case BUTTON_CLOSE:
        return theme.close;

      case BUTTON_MINIMIZE:
        return theme.minimize;

      default:
        return theme.maximize;
    }
}
} // anonymous namespace

decoration_theme_t::decoration_theme_t() :
    theme(shared_theme(theme_directory))
{}

const themed::window_theme_t& decoration_theme_t::values() const
{
    return *theme;
}

int decoration_theme_t::get_title_height() const
{
    return theme->titlebar_height;
}

int decoration_theme_t::get_top_margin() const
{
    // Active and inactive must take the same room, or a window would jump
    // when it gains or loses focus.
    const size_t separator_lines = std::max(theme->titlebar_separator.size(),
        theme->titlebar_separator_inactive.size());
    return theme->titlebar_height + separator_lines;
}

int decoration_theme_t::get_border_size() const
{
    return theme->border_width;
}

int decoration_theme_t::get_resize_grip() const
{
    // The option may ask for a wider strip than the theme's; below 0 means the theme's.
    const int wished = resize_grip_wish;
    return wished >= 0 ? wished : theme->resize_grip;
}

int decoration_theme_t::get_resize_grip_in_title_bar() const
{
    return theme->resize_grip_in_title_bar;
}

int decoration_theme_t::get_resize_corner() const
{
    return theme->resize_corner;
}

namespace
{
// A blurred edge has faded to under half a percent three standard
// deviations out; that is taken as where a shadow ends.
const double SHADOW_END_IN_DEVIATIONS = 3.0;

double deviation_of(const themed::shadow_t& shadow)
{
    // Never zero, so a hard-edged shadow needs no special case.
    return std::max(shadow.blur_radius / 2.0, 0.01);
}

double reach_of(const themed::shadow_t& shadow)
{
    const double farthest_offset = std::max(std::abs(shadow.offset_x), std::abs(shadow.offset_y));
    return std::ceil(SHADOW_END_IN_DEVIATIONS * deviation_of(shadow) + farthest_offset);
}

/** How much a blurred bar from `start` to `end` covers `at`, from 0 to 1. */
double blurred_bar_cover(double at, double start, double end, double deviation)
{
    auto total_up_to = [=] (double distance)
    {
        return 0.5 * (1.0 + std::erf(distance / (deviation * std::sqrt(2.0))));
    };

    return total_up_to(at - start) - total_up_to(at - end);
}
}

bool decoration_theme_t::has_shadow() const
{
    return theme->has_shadow;
}

int decoration_theme_t::get_shadow_reach() const
{
    return (int)std::max(reach_of(theme->shadow), reach_of(theme->shadow_inactive));
}

cairo_surface_t*decoration_theme_t::render_shadow_piece(wf::geometry_t piece, double frame_width,
    double frame_height, bool active) const
{
    const int width  = (int)piece.width;
    const int height = (int)piece.height;
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, std::max(width, 1),
        std::max(height, 1));
    if ((width <= 0) || (height <= 0))
    {
        return surface;
    }

    const themed::shadow_t& shadow = active ? theme->shadow : theme->shadow_inactive;
    const double deviation = deviation_of(shadow);

    // A blurred rectangle is the product of two blurred bars, one across
    // and one down, so each is worked out once per column and per row.
    std::vector<double> across(width);
    for (int column = 0; column < width; column++)
    {
        const double x = piece.x + column + 0.5;
        across[column] = blurred_bar_cover(x, shadow.offset_x, shadow.offset_x + frame_width, deviation);
    }

    std::vector<double> down(height);
    for (int row = 0; row < height; row++)
    {
        const double y = piece.y + row + 0.5;
        down[row] = blurred_bar_cover(y, shadow.offset_y, shadow.offset_y + frame_height, deviation);
    }

    // Cairo's ARGB32: premultiplied, alpha in the top byte.
    cairo_surface_flush(surface);
    unsigned char *pixels = cairo_image_surface_get_data(surface);
    const int bytes_per_row = cairo_image_surface_get_stride(surface);
    for (int row = 0; row < height; row++)
    {
        uint32_t *pixel_row = reinterpret_cast<uint32_t*>(pixels + row * bytes_per_row);
        for (int column = 0; column < width; column++)
        {
            const double opacity = shadow.color.alpha * across[column] * down[row];
            const uint32_t alpha = (uint32_t)std::lround(255.0 * opacity);
            const uint32_t red   = (uint32_t)std::lround(255.0 * opacity * shadow.color.red);
            const uint32_t green = (uint32_t)std::lround(255.0 * opacity * shadow.color.green);
            const uint32_t blue  = (uint32_t)std::lround(255.0 * opacity * shadow.color.blue);
            pixel_row[column] = (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }
    cairo_surface_mark_dirty(surface);

    return surface;
}

bool decoration_theme_t::has_rim(bool active) const
{
    return !(active ? theme->rim : theme->rim_inactive).empty();
}

int decoration_theme_t::get_rim_corner_height() const
{
    return (int)std::ceil(theme->corner_radius);
}

bool decoration_theme_t::get_buttons_hover_together() const
{
    return theme->buttons_hover_together;
}

double decoration_theme_t::get_bottom_corner_radius() const
{
    return theme->bottom_corner_radius;
}

int decoration_theme_t::get_rim_bottom_corner_height() const
{
    return (int)std::ceil(theme->bottom_corner_radius);
}

wf::color_t decoration_theme_t::get_rim_color(rim_part_t part, bool active) const
{
    const themed::rgba_t& color = (active ? theme->rim : theme->rim_inactive)[(size_t)part];
    // Wayfire's colours carry their opacity already multiplied in.
    return wf::color_t{color.red * color.alpha, color.green * color.alpha,
        color.blue * color.alpha, color.alpha};
}

cairo_surface_t*decoration_theme_t::render_rim_top(int width, int height, double scale,
    bool active) const
{
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if ((width <= 0) || (height <= 0))
    {
        return surface;
    }

    auto cr = cairo_create(surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    // The rim's middle is half a line outside the frame, so its corners are
    // that much rounder. The outline is drawn far taller than this picture;
    // only its top and its corners fall inside.
    const double line   = scale;
    const double middle = line / 2.0;
    const double radius = theme->corner_radius * scale + middle;
    themed::rounded_rectangle_path(cr, middle, middle, width - line, height * 4.0,
        {radius, radius, 0.0, 0.0});

    themed::set_source_color(cr,
        (active ? theme->rim : theme->rim_inactive)[(size_t)rim_part_t::TOP]);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);

    cairo_destroy(cr);
    return surface;
}

cairo_surface_t*decoration_theme_t::render_rim_bottom(int width, int height, double scale,
    bool active) const
{
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if ((width <= 0) || (height <= 0))
    {
        return surface;
    }

    auto cr = cairo_create(surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    // As in render_rim_top, with the outline reaching far up instead.
    const double line   = scale;
    const double middle = line / 2.0;
    const double radius = (theme->bottom_corner_radius > 0) ? theme->bottom_corner_radius * scale + middle : 0.0;
    const double outline_height = height * 4.0;
    themed::rounded_rectangle_path(cr, middle, height - middle - outline_height, width - line,
        outline_height, {0.0, 0.0, radius, radius});

    themed::set_source_color(cr,
        (active ? theme->rim : theme->rim_inactive)[(size_t)rim_part_t::BOTTOM]);
    cairo_set_line_width(cr, line);
    cairo_stroke(cr);

    cairo_destroy(cr);
    return surface;
}

void decoration_theme_t::set_tool_bar_joined(bool joined)
{
    tool_bar_joined = joined;
}

void decoration_theme_t::set_buttons(button_type_t flags)
{
    button_flags = flags;
}

wf::color_t decoration_theme_t::get_border_color(bool active) const
{
    const themed::rgba_t& color = active ? theme->border_color : theme->border_color_inactive;
    return wf::color_t{color.red, color.green, color.blue, color.alpha};
}

cairo_surface_t*decoration_theme_t::render_titlebar(int width, int height, double scale,
    bool active) const
{
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if ((width <= 0) || (height <= 0))
    {
        return surface;
    }

    auto cr = cairo_create(surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    const double radius = theme->corner_radius * scale;
    const themed::corner_radii_t top_corners = {radius, radius, 0.0, 0.0};

    cairo_save(cr);
    themed::rounded_rectangle_path(cr, 0, 0, width, height, top_corners);
    cairo_clip(cr);
    const themed::fill_t& background = tool_bar_joined ?
        (active ? theme->titlebar_background_joined : theme->titlebar_background_joined_inactive) :
        (active ? theme->titlebar_background : theme->titlebar_background_inactive);

    // Each thin line is one logical pixel high, in whole device pixels.
    const double line_height = std::max(1.0, std::floor(scale));

    // A gradient ends at the bar's lower edge. The separator rows under it
    // continue with the fill's last row: the fill is drawn once more, moved
    // down, and only its lowest part is let through.
    const double bar_height = theme->titlebar_height * scale;
    themed::paint_fill(cr, background, 0, 0, width, bar_height, scale);
    if (height > bar_height)
    {
        cairo_save(cr);
        cairo_rectangle(cr, 0, bar_height, width, height - bar_height);
        cairo_clip(cr);
        themed::paint_fill(cr, background, 0, height - bar_height, width, bar_height, scale);
        cairo_restore(cr);
    }

    /* Separator lines, stacked upwards, first colour lowest. */
    const auto& separator = tool_bar_joined ?
        (active ? theme->titlebar_separator_joined : theme->titlebar_separator_joined_inactive) :
        (active ? theme->titlebar_separator : theme->titlebar_separator_inactive);
    double line_bottom = height;
    for (const themed::rgba_t& color : separator)
    {
        themed::set_source_color(cr, color);
        cairo_rectangle(cr, 0, line_bottom - line_height, width, line_height);
        cairo_fill(cr);
        line_bottom -= line_height;
    }

    /* Highlight lines, stacked downwards. */
    const auto& highlight =
        active ? theme->titlebar_highlight : theme->titlebar_highlight_inactive;
    double line_top = 0;
    for (const themed::rgba_t& color : highlight)
    {
        themed::set_source_color(cr, color);
        cairo_rectangle(cr, 0, line_top, width, line_height);
        cairo_fill(cr);
        line_top += line_height;
    }

    cairo_restore(cr);

    if (theme->border_width > 0)
    {
        const double line  = theme->border_width * scale;
        const double inset = line / 2.0;
        const double inner_radius = std::max(0.0, radius - inset);

        // Drawn taller than the bar, so only the top and the sides remain.
        themed::rounded_rectangle_path(cr, inset, inset, width - line, height + line,
            {inner_radius, inner_radius, 0.0, 0.0});
        themed::set_source_color(cr,
            active ? theme->border_color : theme->border_color_inactive);
        cairo_set_line_width(cr, line);
        cairo_stroke(cr);
    }

    cairo_destroy(cr);
    return surface;
}

cairo_surface_t*decoration_theme_t::render_text(std::string text, int width, int height,
    double scale, bool active, double preferred_center) const
{
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if ((width <= 0) || (height <= 0))
    {
        return surface;
    }

    auto cr = cairo_create(surface);

    // The theme gives the font size in logical pixels, not points.
    const double font_pixels = theme->title_font_size * scale;

    PangoFontDescription *font = pango_font_description_from_string(
        theme->title_font_family.c_str());
    pango_font_description_set_absolute_size(font, font_pixels * PANGO_SCALE);

    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, text.c_str(), text.size());

    pango_layout_set_width(layout, width * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_single_paragraph_mode(layout, TRUE);

    int text_width, text_height;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    double x = 0;
    switch (theme->title_align)
    {
      case themed::title_align_t::LEFT:
        x = 0;
        break;

      case themed::title_align_t::RIGHT:
        x = width - text_width;
        break;

      case themed::title_align_t::CENTER:
        // Centre on the window, but stay inside the space given.
        x = preferred_center - text_width / 2.0;
        x = std::clamp(x, 0.0, std::max(0.0, (double)(width - text_width)));
        break;
    }

    const double y = (height - text_height) / 2.0;

    themed::set_source_color(cr, active ? theme->title_color : theme->title_color_inactive);
    // Whole pixels, so the text is not blurred.
    cairo_move_to(cr, std::round(x), std::round(y));
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(font);
    g_object_unref(layout);
    cairo_destroy(cr);

    return surface;
}

cairo_surface_t*decoration_theme_t::render_button(button_type_t button,
    themed::button_state_t state, double scale) const
{
    const int width  = std::ceil(theme->button_width * scale);
    const int height = std::ceil(theme->button_height * scale);

    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    auto cr = cairo_create(surface);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    themed::paint_fill(cr, button_theme(*theme, button).look_for(state),
        0, 0, width, height, scale);

    cairo_destroy(cr);
    return surface;
}
}
}
