#pragma once

#include <vector>
#include <wayfire/region.hpp>
#include <linux/input-event-codes.h>
#include "deco-button.hpp"

namespace wf
{
namespace decor
{
static constexpr uint32_t DECORATION_AREA_RENDERABLE_BIT = (1 << 16);
static constexpr uint32_t DECORATION_AREA_RESIZE_BIT     = (1 << 17);
static constexpr uint32_t DECORATION_AREA_MOVE_BIT = (1 << 18);


/** Different types of areas around the decoration */
enum decoration_area_type_t
{
    DECORATION_AREA_MOVE          = DECORATION_AREA_MOVE_BIT,
    DECORATION_AREA_TITLE         =
        DECORATION_AREA_MOVE_BIT | DECORATION_AREA_RENDERABLE_BIT,
    DECORATION_AREA_BUTTON        = DECORATION_AREA_RENDERABLE_BIT,
    DECORATION_AREA_RESIZE_LEFT   = WLR_EDGE_LEFT | DECORATION_AREA_RESIZE_BIT,
    DECORATION_AREA_RESIZE_RIGHT  = WLR_EDGE_RIGHT | DECORATION_AREA_RESIZE_BIT,
    DECORATION_AREA_RESIZE_TOP    = WLR_EDGE_TOP | DECORATION_AREA_RESIZE_BIT,
    DECORATION_AREA_RESIZE_BOTTOM = WLR_EDGE_BOTTOM | DECORATION_AREA_RESIZE_BIT,
};

/** An area of the decoration which reacts to input events. */
struct decoration_area_t
{
  public:
    decoration_area_t(decoration_area_type_t type, wf::geometry_t g);

    /** An area holding a button. */
    decoration_area_t(wf::geometry_t g,
        std::function<void(wf::geometry_t)> damage_callback,
        const decoration_theme_t& theme);

    /** @return The geometry of the decoration area, relative to the layout */
    wf::geometry_t get_geometry() const;

    /** @return The area's button, if the area is a button. Otherwise UB */
    button_t& as_button();

    decoration_area_type_t get_type() const;

  private:
    decoration_area_type_t type;
    wf::geometry_t geometry{};

    /* For buttons only */
    std::unique_ptr<button_t> button;
};


/** Action which needs to be taken in response to an input event */
enum decoration_layout_action_t
{
    DECORATION_ACTION_NONE                     = 0,
    /* Drag actions */
    DECORATION_ACTION_MOVE                     = 1,
    DECORATION_ACTION_RESIZE                   = 2,
    /* Button actions */
    DECORATION_ACTION_CLOSE                    = 3,
    DECORATION_ACTION_TOGGLE_MAXIMIZE          = 4,
    DECORATION_ACTION_MINIMIZE                 = 5,
    DECORATION_ACTION_TOGGLE_MAXIMIZE_VERTICAL = 6,
    DECORATION_ACTION_TOGGLE_MAXIMIZE_HORIZONTAL = 7,
};

class decoration_theme_t;
/** Positions the title and buttons, and dispatches input events to them. */
class decoration_layout_t
{
  public:
    /** When the theme changes, the layout needs to be created again. */
    decoration_layout_t(const decoration_theme_t& theme,
        std::function<void(wf::geometry_t)> damage_callback);

    /** Regenerate layout using the new size */
    void resize(int width, int height);

    /** @return The areas which need to be rendered, top to bottom. */
    std::vector<nonstd::observer_ptr<decoration_area_t>> get_renderable_areas();

    /** @return The combined region of all layout areas */
    wf::regionf_t calculate_region() const;

    struct action_response_t
    {
        decoration_layout_action_t action;
        /* For resizing action, determine the edges for resize request */
        uint32_t edges;
    };

    /** Handle motion event to (x, y) relative to the decoration */
    action_response_t handle_motion(int x, int y);

    action_response_t handle_press_event(bool pressed = true, uint32_t button = BTN_LEFT);

    void handle_focus_lost();

    /** Tell every button whether the window is the active one. */
    void set_window_active(bool is_active);

  private:
    const int titlebar_size;
    const int border_size;
    const decoration_theme_t& theme;

    std::function<void(wf::geometry_t)> damage_callback;

    std::vector<std::unique_ptr<decoration_area_t>> layout_areas;

    bool is_grabbed = false;
    /* Position where the grab has started */
    wf::pointf_t grab_origin;
    /* Last position of the input */
    std::optional<wf::pointf_t> current_input;
    wf::wl_timer<false> timer;
    bool double_click_at_release = false;

    int frame_width  = 0;
    int frame_height = 0;

    void create_resize_areas(int width, int height);

    /** Create buttons in the layout, and return their total geometry */
    wf::geometry_t create_buttons(int width, int height);

    /** Calculate resize edges based on @current_input */
    uint32_t calculate_resize_edges() const;
    /** Update the cursor based on @current_input */
    void update_cursor() const;

    /** @return The layout area at the given coordinates, or null */
    nonstd::observer_ptr<decoration_area_t> find_area_at(std::optional<wf::pointf_t> point);

    /** Unset hover of the button at @position (or all, if they hover together) */
    void unset_hover(std::optional<wf::pointf_t> position);
    void set_all_buttons_hovered(bool hovered);
    /* Button order and side are behaviour, so they are options, not theme. */
    wf::option_wrapper_t<std::string> button_order{"themed-decoration/button_order"};
    wf::option_wrapper_t<std::string> button_side{"themed-decoration/button_side"};
    wf::option_wrapper_t<std::string> title_double_click{"themed-decoration/title_double_click"};
};
}
}
