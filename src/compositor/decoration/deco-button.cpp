#include "deco-button.hpp"
#include "deco-theme.hpp"
#include <wayfire/opengl.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>

namespace wf
{
namespace decor
{
button_t::button_t(const decoration_theme_t& t, std::function<void()> damage) :
    theme(t), damage_callback(damage)
{}

void button_t::set_button_type(button_type_t type)
{
    this->type = type;
    // Forget what was drawn, so that the next render draws the new button.
    this->drawn_scale = 0.0;
    add_idle_damage();
}

button_type_t button_t::get_button_type() const
{
    return this->type;
}

void button_t::set_hover(bool is_hovered)
{
    this->is_hovered = is_hovered;
    add_idle_damage();
}

void button_t::set_pressed(bool is_pressed)
{
    this->is_pressed = is_pressed;
    add_idle_damage();
}

void button_t::set_window_active(bool is_active)
{
    if (this->is_window_active != is_active)
    {
        this->is_window_active = is_active;
        add_idle_damage();
    }
}

themed::button_state_t button_t::current_state() const
{
    // Pressing and hovering win over "window inactive": a button in a
    // background window still responds when the pointer is on it.
    if (is_pressed)
    {
        return themed::button_state_t::PRESSED;
    }

    if (is_hovered)
    {
        return themed::button_state_t::HOVER;
    }

    return is_window_active ?
           themed::button_state_t::NORMAL : themed::button_state_t::INACTIVE;
}

void button_t::render(const scene::render_instruction_t& data, wf::geometry_t geometry)
{
    // Drawn at render time, because only now is the output's scale known.
    update_texture(current_state(), data.target.scale);
    data.pass->add_texture(button_texture.get_texture(), data.target, geometry, data.damage);
}

void button_t::update_texture(themed::button_state_t state, double scale)
{
    const bool already_drawn = (state == drawn_state) && (scale == drawn_scale);
    if (already_drawn)
    {
        return;
    }

    auto surface = theme.render_button(type, state, scale);
    this->button_texture = owned_texture_t{surface};
    cairo_surface_destroy(surface);

    drawn_state = state;
    drawn_scale = scale;
}

void button_t::add_idle_damage()
{
    this->idle_damage.run_once([=] ()
    {
        this->damage_callback();
    });
}

button_t::~button_t()
{}
} // namespace decor
}
