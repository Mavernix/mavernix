// A Wayfire transformer that draws a minimising window as a triangle strip
// between the rows warp-geometry gives.
//
// Adapted from the "squeezimize" effect of Wayfire's animate plugin (see
// UPSTREAM.md), which bends the picture in a shader instead.

#pragma once

#include <wayfire/opengl.hpp>
#include <wayfire/util/duration.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/view.hpp>

#include "warp-geometry.hpp"

namespace themed_minimize
{
enum class effect_t
{
    GENIE,
    SCALE,
};

class warp_transformer_t : public wf::scene::view_2d_transformer_t
{
  public:
    /**
     * @param icon      The dock icon, in output-logical pixels; if empty, the
     *                  middle of the output's bottom edge.
     * @param duration  Its curve should be "linear": the shapes ease themselves.
     */
    warp_transformer_t(wayfire_view view, effect_t effect,
        wf::animation_description_t duration, wf::geometry_t icon);
    ~warp_transformer_t() override;

    void start(bool hiding);
    void reverse();
    bool is_running();

    /** The window's rectangle, frame included, without its shadow. */
    wf::geometry_t window_frame(wf::geometry_t picture_area);

    std::vector<warp::row_t> current_rows(wf::geometry_t window);

    wf::geometry_t get_bounding_box() override;
    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
        wf::scene::damage_callback push_damage, wf::output_t *shown_on) override;

    OpenGL::program_t program;
    // The window and the icon.
    wf::geometry_t affected_area;

  private:
    std::weak_ptr<wf::view_interface_t> _view;
    effect_t effect;
    wf::geometry_t icon;
    // Runs from 0 (the window as it is) to 1 (inside the icon).
    wf::animation::simple_animation_t progress;
};
}
