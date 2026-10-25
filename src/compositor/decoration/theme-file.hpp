#pragma once

/**
 * The window part of a theme (docs/themes/), read from `theme.toml` into
 * plain, checked data. Knows nothing about Wayfire or drawing.
 */

#include <array>
#include <map>
#include <string>
#include <vector>

namespace themed
{
/** A colour with opacity. Each value is between 0.0 and 1.0. */
struct rgba_t
{
    double red   = 0.0;
    double green = 0.0;
    double blue  = 0.0;
    double alpha = 1.0;
};

/** One colour of a gradient and where it sits: 0.0 is the top, 1.0 the bottom. */
struct gradient_stop_t
{
    double position;
    rgba_t color;
};

/** A picture laid over a fill, repeated to cover it: a grain or texture. */
struct overlay_t
{
    /** How the picture's pixels combine with what is underneath. */
    enum class blend_t
    {
        NORMAL,  /** simply drawn on top */
        OVERLAY, /** the usual way to add grain */
        MULTIPLY,
        SCREEN,
    };

    /** Full path of the picture for scale 1; empty means no overlay. */
    std::string image_path;
    double opacity = 1.0;
    blend_t blend  = blend_t::NORMAL;
};

/** How an area is filled: a colour, a top-to-bottom gradient, or a stretched picture. */
struct fill_t
{
    enum class kind_t
    {
        NONE, /** nothing is drawn */
        COLOR,
        GRADIENT,
        IMAGE,
    };

    kind_t kind = kind_t::NONE;

    rgba_t color;

    /** For GRADIENT: at least two stops, ordered from top to bottom. */
    std::vector<gradient_stop_t> gradient;

    /** For IMAGE: path for scale 1; a "@2x" file beside it serves double density. */
    std::string image_path;

    /** For IMAGE: top, right, bottom, left edges (scale-1 pixels) that do not stretch. */
    std::array<int, 4> slice = {0, 0, 0, 0};

    /** For COLOR and GRADIENT: corner rounding, in logical pixels. */
    double radius = 0.0;

    /** For COLOR and GRADIENT: an outline. Width 0 means none. */
    double border_width = 0.0;
    rgba_t border_color;

    overlay_t overlay;
};

enum class button_state_t
{
    NORMAL,
    HOVER,
    PRESSED,
    INACTIVE, /** the button's window is not the active one */
};

/** One window button: a fill for each state. */
struct button_theme_t
{
    std::map<button_state_t, fill_t> looks;

    /** A state the theme leaves out looks like NORMAL. */
    const fill_t& look_for(button_state_t state) const;
};

enum class title_align_t
{
    LEFT,
    CENTER,
    RIGHT,
};

struct shadow_t
{
    rgba_t color;
    // How soft the edge is, as in CSS: twice the blur's standard deviation.
    double blur_radius = 0.0;
    double offset_x    = 0.0;
    double offset_y    = 0.0;
};

/** Everything a theme says about window frames. Lengths are logical pixels. */
struct window_theme_t
{
    /* Title bar */
    int titlebar_height = 0;
    fill_t titlebar_background;
    fill_t titlebar_background_inactive;
    /**
     * One-pixel lines between the bar and the content, lowest first. They
     * add to the frame's height; the bar's fill continues behind them.
     */
    std::vector<rgba_t> titlebar_separator;
    std::vector<rgba_t> titlebar_separator_inactive;
    /** One-pixel lines drawn over the bar's first rows, topmost first. */
    std::vector<rgba_t> titlebar_highlight;
    std::vector<rgba_t> titlebar_highlight_inactive;
    /** For a window whose tool bar joins the title bar; default to the plain ones. */
    fill_t titlebar_background_joined;
    fill_t titlebar_background_joined_inactive;
    std::vector<rgba_t> titlebar_separator_joined;
    std::vector<rgba_t> titlebar_separator_joined_inactive;

    /* Title text */
    rgba_t title_color;
    rgba_t title_color_inactive;
    title_align_t title_align = title_align_t::CENTER;
    int title_padding = 0;
    std::string title_font_family;
    double title_font_size = 0.0;

    /* Buttons */
    int button_width  = 0;
    int button_height = 0;
    int button_spacing = 0;
    int button_edge_inset = 0;
    bool buttons_hover_together = false;
    button_theme_t close;
    button_theme_t minimize;
    button_theme_t maximize;

    /* Frame around the window's content */
    int border_width = 0;
    /** An undrawn strip outside the edge that still resizes; needed when border_width is 0. */
    int resize_grip = 4;
    /** Rows at the title bar's top that resize instead of moving the window. */
    int resize_grip_in_title_bar = 0;
    /** How far from a corner a press takes both edges; 0: only where the strips cross. */
    int resize_corner = 0;
    rgba_t border_color;
    rgba_t border_color_inactive;
    double corner_radius = 0.0;
    /** The bottom corners belong to the program's picture and are cut round (corner-clip.hpp). */
    double bottom_corner_radius = 0.0;

    /** A soft copy of the frame's rectangle, moved by the offset. */
    bool has_shadow = false;
    shadow_t shadow;
    shadow_t shadow_inactive;

    /**
     * A one-pixel hairline just outside the frame (top, sides, bottom), drawn
     * over whatever is next to the window. Empty for none.
     */
    std::vector<rgba_t> rim;
    std::vector<rgba_t> rim_inactive;
};

/** A rim lists this many colours: top, sides, bottom. */
const size_t RIM_COLOR_COUNT = 3;

/**
 * Throws std::runtime_error naming the file, key and problem. Unknown keys
 * are an error, so that a misspelt key is noticed.
 */
window_theme_t load_window_theme(const std::string& theme_directory);

/** A plain grey look that needs no files, for when the theme cannot be loaded. */
window_theme_t fallback_window_theme();
}
