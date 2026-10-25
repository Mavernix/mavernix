// Rounds the bottom corners of a decorated window, which are the program's
// own picture.
//
// A transformer that draws the whole window into a buffer would copy every
// window on every frame. This one only takes the two bottom corner squares
// through a buffer and a shader that cuts the rounding. Each square is drawn
// twice, the frame alone and then the window cut round over it, so the
// frame's shadow and rim still show outside the rounding.

#pragma once

#include <wayfire/opengl.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/toplevel-view.hpp>

namespace wf
{
class corner_clip_transformer_t : public wf::scene::transformer_base_node_t
{
  public:
    /**
     * @param radius  In logical pixels.
     * @param frame   The window's decoration node, drawn uncut outside the rounding.
     */
    corner_clip_transformer_t(wayfire_toplevel_view view, double radius,
        std::shared_ptr<wf::scene::node_t> frame);
    ~corner_clip_transformer_t() override;

    /** Whether the shader could be prepared: only with the GLES renderer. */
    bool can_draw() const;

    /** In the same coordinates as the window. */
    std::vector<wf::geometry_t> corner_squares() const;

    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
        wf::scene::damage_callback push_damage, wf::output_t *shown_on) override;

    std::string stringify() const override;

    double radius;
    OpenGL::program_t program;
    // Per corner: the whole window, and the frame alone.
    wf::auxilliary_buffer_t window_buffers[2];
    wf::auxilliary_buffer_t frame_buffers[2];
    std::weak_ptr<wf::scene::node_t> frame;

  private:
    std::weak_ptr<wf::toplevel_view_interface_t> _view;
    bool program_ready = false;
};
}
