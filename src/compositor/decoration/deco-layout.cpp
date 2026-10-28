#include "deco-layout.hpp"
#include "deco-theme.hpp"
#include <wayfire/core.hpp>
#include <wayfire/nonstd/reverse.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/util.hpp>
#include <sstream>

namespace wf
{
namespace decor
{
decoration_area_t::decoration_area_t(decoration_area_type_t type, wf::geometry_t g)
{
    this->type     = type;
    this->geometry = g;

    assert(type != DECORATION_AREA_BUTTON);
}

decoration_area_t::decoration_area_t(wf::geometry_t g,
    std::function<void(wf::geometry_t)> damage_callback,
    const decoration_theme_t& theme)
{
    this->type     = DECORATION_AREA_BUTTON;
    this->geometry = g;

    this->button = std::make_unique<button_t>(theme,
        std::bind(damage_callback, g));
}

wf::geometry_t decoration_area_t::get_geometry() const
{
    return geometry;
}

button_t& decoration_area_t::as_button()
{
    assert(button);

    return *button;
}

decoration_area_type_t decoration_area_t::get_type() const
{
    return type;
}

decoration_layout_t::decoration_layout_t(const decoration_theme_t& th,
    std::function<void(wf::geometry_t)> callback) :

    titlebar_size(th.get_title_height()),
    border_size(th.get_border_size()),
    theme(th),
    damage_callback(callback)
{}

/** @return The strip the buttons occupy, including the gap to the window's edge. */
wf::geometry_t decoration_layout_t::create_buttons(int width, int)
{
    std::stringstream stream((std::string)button_order);
    std::vector<button_type_t> buttons;
    std::string button_name;
    while (stream >> button_name)
    {
        // A window that cannot be minimized (a dialog) still shows the
        // button, disabled, as on the original.
        if (button_name == "minimize")
        {
            buttons.push_back(BUTTON_MINIMIZE);
        }

        if ((button_name == "maximize") && (theme.button_flags & BUTTON_TOGGLE_MAXIMIZE))
        {
            buttons.push_back(BUTTON_TOGGLE_MAXIMIZE);
        }

        if ((button_name == "close") && (theme.button_flags & BUTTON_CLOSE))
        {
            buttons.push_back(BUTTON_CLOSE);
        }
    }

    const themed::window_theme_t& looks = theme.values();
    const int count = buttons.size();

    const int row_width = count * looks.button_width +
        std::max(0, count - 1) * looks.button_spacing;
    const int strip_width = (count > 0) ? row_width + looks.button_edge_inset : 0;

    const bool on_left = ((std::string)button_side == "left");
    const int strip_x  = on_left ? 0 : width - strip_width;

    const int button_y = (titlebar_size - looks.button_height) / 2;
    int button_x = on_left ? looks.button_edge_inset : strip_x;

    for (auto type : buttons)
    {
        wf::geometry_t button_geometry = {
            (double)button_x, (double)button_y,
            (double)looks.button_width, (double)looks.button_height,
        };

        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            button_geometry, damage_callback, theme));
        this->layout_areas.back()->as_button().set_button_type(type);
        const bool disabled = (fixed_size && (type == BUTTON_TOGGLE_MAXIMIZE)) ||
            ((type == BUTTON_MINIMIZE) && !(theme.button_flags & BUTTON_MINIMIZE));
        this->layout_areas.back()->as_button().set_enabled(!disabled);

        button_x += looks.button_width + looks.button_spacing;
    }

    return {
        (double)strip_x, 0.0,
        (double)strip_width, (double)titlebar_size
    };
}

/**
 * Each edge is the border line plus an undrawn grip outside it; only at the
 * title bar may it reach inwards. The strips overlap at the corners, so a
 * press there resizes both ways.
 */
void decoration_layout_t::create_resize_areas(int width, int height)
{
    const double grip  = theme.get_resize_grip();
    const double strip = border_size + grip;
    const double top_strip = std::max<double>(border_size,
        titlebar_size > 0 ? theme.get_resize_grip_in_title_bar() : 0) + grip;

    wf::geometry_t border_geometry = {-grip, -grip, strip, (double)height + 2 * grip};
    this->layout_areas.push_back(std::make_unique<decoration_area_t>(
        DECORATION_AREA_RESIZE_LEFT, border_geometry));

    border_geometry = {(double)(width - border_size), -grip, strip, (double)height + 2 * grip};
    this->layout_areas.push_back(std::make_unique<decoration_area_t>(
        DECORATION_AREA_RESIZE_RIGHT, border_geometry));

    border_geometry = {-grip, -grip, (double)width + 2 * grip, top_strip};
    this->layout_areas.push_back(std::make_unique<decoration_area_t>(
        DECORATION_AREA_RESIZE_TOP, border_geometry));

    border_geometry = {-grip, (double)(height - border_size), (double)width + 2 * grip, strip};
    this->layout_areas.push_back(std::make_unique<decoration_area_t>(
        DECORATION_AREA_RESIZE_BOTTOM, border_geometry));
}

void decoration_layout_t::resize(int width, int height)
{
    this->layout_areas.clear();
    this->frame_width  = width;
    this->frame_height = height;

    // Resize strips first: find_area_at takes the first match, so they win
    // over the title bar.
    if (!fixed_size)
    {
        create_resize_areas(width, height);
    }

    if (this->titlebar_size > 0)
    {
        auto button_strip = create_buttons(width, height);

        /* The strip around the buttons, allows move */
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_MOVE, button_strip));

        /* The rest of the title bar shows the title, and allows move. */
        const bool buttons_on_left = (button_strip.x == 0);
        const double free_start = buttons_on_left ? button_strip.width : 0.0;
        const double free_width = (double)width - button_strip.width;

        const double padding = theme.values().title_padding;
        wf::geometry_t title_geometry = {
            free_start + padding,
            0.0,
            std::max(0.0, free_width - 2 * padding),
            (double)titlebar_size,
        };
        this->layout_areas.push_back(std::make_unique<decoration_area_t>(
            DECORATION_AREA_TITLE, title_geometry));
    }
}

std::vector<nonstd::observer_ptr<decoration_area_t>> decoration_layout_t::get_renderable_areas()
{
    std::vector<nonstd::observer_ptr<decoration_area_t>> renderable;
    for (auto& area : layout_areas)
    {
        if (area->get_type() & DECORATION_AREA_RENDERABLE_BIT)
        {
            renderable.push_back({area});
        }
    }

    return renderable;
}

wf::regionf_t decoration_layout_t::calculate_region() const
{
    wf::regionf_t r{};
    for (auto& area : layout_areas)
    {
        auto g = area->get_geometry();
        if ((g.width > 0) && (g.height > 0))
        {
            r |= g;
        }
    }

    return r;
}

void decoration_layout_t::unset_hover(std::optional<wf::pointf_t> position)
{
    if (theme.get_buttons_hover_together())
    {
        set_all_buttons_hovered(false);
        return;
    }

    auto area = find_area_at(position);
    if (area && (area->get_type() == DECORATION_AREA_BUTTON))
    {
        area->as_button().set_hover(false);
    }
}

void decoration_layout_t::set_all_buttons_hovered(bool hovered)
{
    for (auto& area : layout_areas)
    {
        if (area->get_type() == DECORATION_AREA_BUTTON)
        {
            area->as_button().set_hover(hovered);
        }
    }
}

decoration_layout_t::action_response_t decoration_layout_t::handle_motion(
    int x, int y)
{
    auto previous_area = find_area_at(current_input);
    auto current_area  = find_area_at(wf::pointf_t{(double)x, (double)y});

    // Dragging from a part that moves the window starts a move, wherever the
    // pointer is now (a quick drag can leave the title bar in one step). Not
    // during a double press: a touchpad's double tap holds its second press,
    // and the least finger movement would drag the window away.
    auto pressed_area = is_grabbed ? find_area_at(grab_origin) : nullptr;
    if (pressed_area && (pressed_area->get_type() & DECORATION_AREA_MOVE_BIT) &&
        !double_click_at_release)
    {
        is_grabbed = false;
        return {DECORATION_ACTION_MOVE, 0};
    }

    if (previous_area != current_area)
    {
        const bool on_button = current_area && (current_area->get_type() == DECORATION_AREA_BUTTON);
        unset_hover(current_input);
        if (on_button && theme.get_buttons_hover_together())
        {
            set_all_buttons_hovered(true);
        } else if (on_button)
        {
            current_area->as_button().set_hover(true);
        }
    }

    this->current_input = wf::pointf_t{(double)x, (double)y};
    update_cursor();

    return {DECORATION_ACTION_NONE, 0};
}

decoration_layout_t::action_response_t decoration_layout_t::handle_press_event(
    bool pressed, uint32_t button)
{
    if (pressed)
    {
        auto area = find_area_at(current_input);
        if (area && (area->get_type() & DECORATION_AREA_MOVE_BIT))
        {
            if (timer.is_connected())
            {
                double_click_at_release = true;
            } else
            {
                timer.set_timeout(300, [] () { return false; });
            }
        }

        if (area && (area->get_type() & DECORATION_AREA_RESIZE_BIT) && (button == BTN_LEFT))
        {
            return {DECORATION_ACTION_RESIZE, calculate_resize_edges()};
        }

        if (area && (area->get_type() == DECORATION_AREA_BUTTON))
        {
            area->as_button().set_pressed(true);
        }

        is_grabbed  = true;
        grab_origin = current_input.value_or(wf::pointf_t{0.0, 0.0});
    }

    if (!pressed && double_click_at_release)
    {
        double_click_at_release = false;
        is_grabbed = false;
        if (button == BTN_LEFT)
        {
            // "maximize", "minimize", or anything else for nothing.
            const std::string wanted = title_double_click;
            if ((wanted == "maximize") && !fixed_size)
            {
                return {DECORATION_ACTION_TOGGLE_MAXIMIZE, 0};
            }

            if ((wanted == "minimize") && (theme.button_flags & BUTTON_MINIMIZE))
            {
                return {DECORATION_ACTION_MINIMIZE, 0};
            }

            return {DECORATION_ACTION_NONE, 0};
        }
    } else if (!pressed && is_grabbed)
    {
        is_grabbed = false;
        auto begin_area = find_area_at(grab_origin);
        auto end_area   = find_area_at(current_input);

        if (begin_area && (begin_area->get_type() == DECORATION_AREA_BUTTON))
        {
            begin_area->as_button().set_pressed(false);
            if (end_area && (begin_area == end_area) && begin_area->as_button().is_enabled())
            {
                switch (begin_area->as_button().get_button_type())
                {
                  case BUTTON_CLOSE:
                    if (button == BTN_LEFT)
                    {
                        return {DECORATION_ACTION_CLOSE, 0};
                    }

                    break;

                  case BUTTON_TOGGLE_MAXIMIZE:
                    if (button == BTN_LEFT)
                    {
                        return {DECORATION_ACTION_TOGGLE_MAXIMIZE, 0};
                    } else if (button == BTN_MIDDLE)
                    {
                        return {DECORATION_ACTION_TOGGLE_MAXIMIZE_VERTICAL, 0};
                    } else if (button == BTN_RIGHT)
                    {
                        return {DECORATION_ACTION_TOGGLE_MAXIMIZE_HORIZONTAL, 0};
                    }

                    break;

                  case BUTTON_MINIMIZE:
                    if (button == BTN_LEFT)
                    {
                        return {DECORATION_ACTION_MINIMIZE, 0};
                    }

                    break;

                  default:
                    break;
                }
            }
        }
    }

    return {DECORATION_ACTION_NONE, 0};
}

nonstd::observer_ptr<decoration_area_t> decoration_layout_t::find_area_at(std::optional<wf::pointf_t> point)
{
    if (!point)
    {
        return nullptr;
    }

    for (auto& area : this->layout_areas)
    {
        if (area->get_geometry() & *point)
        {
            return {area};
        }
    }

    return nullptr;
}

uint32_t decoration_layout_t::calculate_resize_edges() const
{
    if (!this->current_input.has_value())
    {
        return 0;
    }

    uint32_t edges = 0;
    for (auto& area : layout_areas)
    {
        if (area->get_geometry() & *this->current_input)
        {
            if (area->get_type() & DECORATION_AREA_RESIZE_BIT)
            {
                edges |= (area->get_type() & ~DECORATION_AREA_RESIZE_BIT);
            }
        }
    }

    // Near a corner a press on one edge takes hold of the other as well.
    const double corner = theme.get_resize_corner();
    const wf::pointf_t at = *this->current_input;
    if (edges & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT))
    {
        if (at.y < corner)
        {
            edges |= WLR_EDGE_TOP;
        } else if (at.y >= frame_height - corner)
        {
            edges |= WLR_EDGE_BOTTOM;
        }
    }

    if (edges & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM))
    {
        if (at.x < corner)
        {
            edges |= WLR_EDGE_LEFT;
        } else if (at.x >= frame_width - corner)
        {
            edges |= WLR_EDGE_RIGHT;
        }
    }

    return edges;
}

void decoration_layout_t::update_cursor() const
{
    uint32_t edges   = calculate_resize_edges();
    auto cursor_name = edges > 0 ?
        wlr_xcursor_get_resize_name((wlr_edges)edges) : "default";
    wf::get_core().set_cursor(cursor_name);
}

void decoration_layout_t::set_window_active(bool is_active)
{
    for (auto& area : layout_areas)
    {
        if (area->get_type() == DECORATION_AREA_BUTTON)
        {
            area->as_button().set_window_active(is_active);
        }
    }
}

void decoration_layout_t::set_fixed_size(bool is_fixed)
{
    if (this->fixed_size == is_fixed)
    {
        return;
    }

    // The areas are made anew: the resize strips come or go, and the
    // maximize button is made with the new state.
    this->fixed_size = is_fixed;
    resize(frame_width, frame_height);
    damage_callback({0.0, 0.0, (double)frame_width, (double)frame_height});
}

void decoration_layout_t::handle_focus_lost()
{
    if (is_grabbed)
    {
        this->is_grabbed = false;
        auto area = find_area_at(grab_origin);
        if (area && (area->get_type() == DECORATION_AREA_BUTTON))
        {
            area->as_button().set_pressed(false);
        }
    }

    this->unset_hover(current_input);
}
}
}
