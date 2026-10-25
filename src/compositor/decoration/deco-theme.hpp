#pragma once
#include <memory>
#include <string>

#include <wayfire/option-wrapper.hpp>
#include <wayfire/config/types.hpp>

#include "deco-button.hpp"
#include "theme-file.hpp"

namespace wf
{
namespace decor
{
/**
 * Holds the window theme and draws the pieces of a window frame from it.
 * Every render_* function returns a cairo surface in device pixels, which
 * the caller turns into a texture and frees.
 */
class decoration_theme_t
{
  public:
    /** Loads the theme option's theme, or a plain built-in look if that fails. */
    decoration_theme_t();

    /** The theme's values, for code that lays things out. */
    const themed::window_theme_t& values() const;

    /** @return The height of the title bar, in logical pixels */
    int get_title_height() const;
    /** @return The title bar plus the separator lines under it */
    int get_top_margin() const;
    /** @return The width of the frame line around the window */
    int get_border_size() const;
    /** @return How far outside the window's edge a press still resizes it */
    int get_resize_grip() const;
    /** @return How many rows at the title bar's top resize instead of moving */
    int get_resize_grip_in_title_bar() const;
    /** @return How far from a corner a press takes hold of both edges */
    int get_resize_corner() const;
    /** @return Whether the pointer on one button shows all as hovered */
    bool get_buttons_hover_together() const;

    /** Set which buttons this window has */
    void set_buttons(button_type_t flags);
    button_type_t button_flags;
    bool tool_bar_joined = false;

    /** The colour of the frame line beside and below the window's content. */
    wf::color_t get_border_color(bool active) const;

    /** @return Whether the theme gives windows a shadow. */
    bool has_shadow() const;
    /** @return How far beyond the frame a shadow can reach, in logical pixels. */
    int get_shadow_reach() const;

    /**
     * Draw one rectangular piece of a window's shadow, measured in logical
     * pixels from the frame's top left. One pixel per logical pixel at any
     * density: a shadow is soft enough to be enlarged.
     */
    cairo_surface_t *render_shadow_piece(wf::geometry_t piece, double frame_width,
        double frame_height, bool active) const;

    /** The three parts of the rim, in the order a theme lists their colours. */
    enum class rim_part_t
    {
        TOP    = 0,
        SIDES  = 1,
        BOTTOM = 2,
    };

    /** @return Whether the theme has a rim for windows in this state. */
    bool has_rim(bool active) const;
    int get_rim_corner_height() const;
    /** A tool bar right under the title bar takes the theme's "joined" look. */
    void set_tool_bar_joined(bool joined);

    /** @return The radius of the window's bottom corners, in logical pixels (0: square). */
    double get_bottom_corner_radius() const;
    int get_rim_bottom_corner_height() const;
    /** The colour of one part of the rim. Only valid if has_rim(). */
    wf::color_t get_rim_color(rim_part_t part, bool active) const;

    /** The rim's top line and rounded corners; height is rim plus get_rim_corner_height(). */
    cairo_surface_t *render_rim_top(int width, int height, double scale, bool active) const;

    /** The rim's bottom, like the top turned upside down. */
    cairo_surface_t *render_rim_bottom(int width, int height, double scale, bool active) const;

    /** The title bar's fill, clipped to rounded top corners, with its frame line. */
    cairo_surface_t *render_titlebar(int width, int height, double scale, bool active) const;

    /**
     * @param preferred_center Where a centred title's middle should be: the
     *   window's middle, not the middle of the space beside the buttons.
     */
    cairo_surface_t *render_text(std::string text, int width, int height, double scale,
        bool active, double preferred_center) const;

    cairo_surface_t *render_button(button_type_t button, themed::button_state_t state,
        double scale) const;

  private:
    // Order matters: `theme` is loaded from `theme_directory`, so that
    // option must be constructed first.
    wf::option_wrapper_t<std::string> theme_directory{"themed-decoration/theme"};
    wf::option_wrapper_t<int> resize_grip_wish{"themed-decoration/resize_grip"};
    std::shared_ptr<const themed::window_theme_t> theme;
};
}
}
