#pragma once

#include <wayfire/scene-render.hpp>
#include <wayfire/util.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/util/duration.hpp>
#include <wayfire/plugins/common/cairo-util.hpp>

#include <cairo.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "theme-file.hpp"

namespace wf
{
namespace decor
{
class decoration_theme_t;

enum button_type_t
{
    BUTTON_CLOSE           = 1 << 0,
    BUTTON_TOGGLE_MAXIMIZE = 1 << 1,
    BUTTON_MINIMIZE        = 1 << 2,
};

class button_t
{
  public:
    /** Damage won't be reported while render() is being called. */
    button_t(const decoration_theme_t& theme,
        std::function<void()> damage_callback);

    ~button_t();
    button_t(const button_t &) = delete;
    button_t(button_t &&) = delete;
    button_t& operator =(const button_t&) = delete;
    button_t& operator =(button_t&&) = delete;

    void set_button_type(button_type_t type);

    button_type_t get_button_type() const;

    void set_hover(bool is_hovered);

    void set_pressed(bool is_pressed);

    void set_window_active(bool is_active);

    /** A disabled button is drawn greyed and takes no hover or press. */
    void set_enabled(bool is_enabled);

    bool is_enabled() const;

    /** Geometry in logical coordinates. A no-op before set_button_type(). */
    void render(const scene::render_instruction_t& data, wf::geometry_t geometry);

  private:
    const decoration_theme_t& theme;

    button_type_t type;
    wf::owned_texture_t button_texture;

    /* What the texture currently shows, so it is only redrawn on a change. */
    themed::button_state_t drawn_state = themed::button_state_t::NORMAL;
    double drawn_scale = 0.0;

    bool is_hovered = false;
    bool is_pressed = false;
    bool is_window_active = true;
    bool enabled = true;

    std::function<void()> damage_callback;
    wf::wl_idle_call idle_damage;
    void add_idle_damage();

    themed::button_state_t current_state() const;

    void update_texture(themed::button_state_t state, double scale);
};
}
}
