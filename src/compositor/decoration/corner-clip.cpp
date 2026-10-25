#include "corner-clip.hpp"

#include <cmath>

#include <wayfire/scene-render.hpp>
#include <wayfire/render.hpp>

namespace wf
{
namespace
{
const char *vertex_shader =
    R"(
#version 100

attribute highp vec2 position;
attribute highp vec2 picture_position_in;
attribute highp vec2 from_circle_middle_in;

uniform mat4 matrix;

varying highp vec2 picture_position;
varying highp vec2 from_circle_middle;

void main() {
    picture_position = picture_position_in;
    from_circle_middle = from_circle_middle_in;
    gl_Position = matrix * vec4(position, 0.0, 1.0);
}
)";

// Wayfire fills in @builtin_ext@ and @builtin@, which give get_pixel() for
// any kind of texture. The edge is antialiased over one device pixel; colours
// are premultiplied, so all four channels are scaled alike.
const char *fragment_shader =
    R"(
#version 100
@builtin_ext@
@builtin@

precision highp float;

uniform float radius;
uniform float device_pixels_per_point;
// 1.0 to cut round, 0.0 to draw the square as it is (the frame).
uniform float cut;

varying highp vec2 picture_position;
varying highp vec2 from_circle_middle;

void main()
{
    float distance_outside = length(from_circle_middle) - radius;
    float coverage = clamp(0.5 - distance_outside * device_pixels_per_point, 0.0, 1.0);
    coverage = mix(1.0, coverage, cut);
    gl_FragColor = get_pixel(picture_position) * coverage;
}
)";

const int BOTTOM_LEFT  = 0;
const int BOTTOM_RIGHT = 1;

class corner_clip_render_instance_t :
    public wf::scene::transformer_render_instance_t<corner_clip_transformer_t>
{
    // The frame alone, for the corners. Its damage already arrives through
    // the window's own instances.
    std::vector<wf::scene::render_instance_uptr> frame_instances;

  public:
    corner_clip_render_instance_t(corner_clip_transformer_t *self,
        wf::scene::damage_callback push_damage, wf::output_t *shown_on) :
        transformer_render_instance_t(self, push_damage, shown_on)
    {
        if (auto frame = self->frame.lock())
        {
            frame->gen_render_instances(frame_instances, [] (const wf::regionf_t&) {}, shown_on);
        }
    }

    /**
     * The window draws itself everywhere but in the two corner squares; this
     * instance draws those. Wayfire lets each part take from `damage` what it
     * covers completely; the corners are not opaque, so they are handed back
     * for the parts behind.
     */
    void schedule_instructions(std::vector<wf::scene::render_instruction_t>& instructions,
        const wf::render_target_t& target, wf::regionf_t& damage) override
    {
        wf::regionf_t corners;
        for (const wf::geometry_t& square : self->corner_squares())
        {
            corners |= square;
        }

        wf::regionf_t corner_damage = damage & corners;
        if (!corner_damage.empty())
        {
            instructions.push_back(wf::scene::render_instruction_t{
                        .instance = this,
                        .target   = target,
                        .damage   = corner_damage,
                    });
        }

        // (For regions, ^ is "without".)
        wf::regionf_t elsewhere = damage ^ corners;
        for (auto& child : children)
        {
            child->schedule_instructions(instructions, target, elsewhere);
        }

        damage = elsewhere | corner_damage;
    }

    void render(const wf::scene::render_instruction_t& data) override
    {
        const std::vector<wf::geometry_t> squares = self->corner_squares();
        for (int corner : {BOTTOM_LEFT, BOTTOM_RIGHT})
        {
            const wf::geometry_t& square = squares[corner];
            const wf::regionf_t to_draw  = data.damage & square;
            if (to_draw.empty())
            {
                continue;
            }

            wf::auxilliary_buffer_t& frame_buffer = self->frame_buffers[corner];
            draw_into(frame_buffer, frame_instances, square + frame_origin(), data.target.scale);
            draw_square(frame_buffer, square, corner, false, data, to_draw);

            wf::auxilliary_buffer_t& window_buffer = self->window_buffers[corner];
            draw_into(window_buffer, children, square, data.target.scale);
            draw_square(window_buffer, square, corner, true, data, to_draw);
        }
    }

  private:
    /**
     * The offset from the window's coordinates to the frame's. The frame's
     * instances made here bypass the surface root, which moves its children
     * to where the window's contents begin.
     */
    wf::pointf_t frame_origin()
    {
        auto frame = self->frame.lock();
        if (!frame || !frame->parent())
        {
            return {0, 0};
        }

        return frame->parent()->to_local({0, 0});
    }

    void draw_into(wf::auxilliary_buffer_t& buffer,
        std::vector<wf::scene::render_instance_uptr>& instances, wf::geometry_t square, float scale)
    {
        buffer.allocate(wf::dimensions(square), scale);

        wf::render_pass_params_t params;
        params.instances = &instances;
        params.target    = wf::render_target_t{buffer};
        params.target.geometry = square;
        params.target.scale    = scale;
        params.damage = wf::regionf_t{square};
        params.background_color = {0.0, 0.0, 0.0, 0.0};
        params.reference_output = _shown_on;
        params.flags = wf::RPASS_CLEAR_BACKGROUND;
        wf::render_pass_t::run(params);
    }

    /** Draws `buffer` onto the screen at `square`, cut round at its corner if `cut`. */
    void draw_square(wf::auxilliary_buffer_t& buffer, wf::geometry_t square, int corner, bool cut,
        const wf::scene::render_instruction_t& data, const wf::regionf_t& to_draw)
    {
        // The circle's middle is the square's inner corner: its top right
        // for the left corner, its top left for the right one.
        const double middle_x = (corner == BOTTOM_LEFT) ? square.x + square.width : square.x;
        const double middle_y = square.y;

        const double left   = square.x;
        const double top    = square.y;
        const double right  = square.x + square.width;
        const double bottom = square.y + square.height;

        // A triangle strip: top left, top right, bottom left, bottom right.
        // In a texture, height 1 is the top edge.
        const float positions[] = {
            (float)left, (float)top, (float)right, (float)top,
            (float)left, (float)bottom, (float)right, (float)bottom,
        };
        const float picture_positions[] = {
            0.0f, 1.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
        };
        const float from_middle[] = {
            (float)(left - middle_x), (float)(top - middle_y),
            (float)(right - middle_x), (float)(top - middle_y),
            (float)(left - middle_x), (float)(bottom - middle_y),
            (float)(right - middle_x), (float)(bottom - middle_y),
        };

        auto picture = wf::gles_texture_t{wf::texture_t::from_aux(buffer)};
        data.pass->custom_gles_subpass(data.target, [&]
        {
            self->program.use(wf::TEXTURE_TYPE_RGBA);
            self->program.uniformMatrix4f("matrix",
                wf::gles::render_target_orthographic_projection(data.target));
            self->program.uniform1f("radius", self->radius);
            self->program.uniform1f("device_pixels_per_point", data.target.scale);
            self->program.uniform1f("cut", cut ? 1.0f : 0.0f);
            self->program.attrib_pointer("position", 2, 0, positions);
            self->program.attrib_pointer("picture_position_in", 2, 0, picture_positions);
            self->program.attrib_pointer("from_circle_middle_in", 2, 0, from_middle);
            self->program.set_active_texture(picture);

            // Premultiplied colours.
            GL_CALL(glEnable(GL_BLEND));
            GL_CALL(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
            wf::gles::for_each_scissor_rect(data.target, to_draw, [&]
            {
                GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
            });
        });
    }
};
}

corner_clip_transformer_t::corner_clip_transformer_t(wayfire_toplevel_view view, double radius,
    std::shared_ptr<wf::scene::node_t> frame) :
    wf::scene::transformer_base_node_t(false), radius(radius), frame(frame), _view(view->weak_from_this())
{
    // Only possible with the GLES renderer; otherwise windows keep square
    // corners (see can_draw).
    wf::gles::run_in_context_if_gles([&]
    {
        program.compile(vertex_shader, fragment_shader);
        program_ready = true;
    });
}

corner_clip_transformer_t::~corner_clip_transformer_t()
{
    wf::gles::run_in_context_if_gles([&]
    {
        program.free_resources();
    });
}

bool corner_clip_transformer_t::can_draw() const
{
    return program_ready;
}

std::vector<wf::geometry_t> corner_clip_transformer_t::corner_squares() const
{
    auto view = _view.lock();
    if (!view)
    {
        return {{0, 0, 0, 0}, {0, 0, 0, 0}};
    }

    // The frame has no border at the bottom, so its bottom edge is the contents'.
    const wf::geometry_t frame = view->get_geometry();
    const double size = std::ceil(radius);
    return {
        {frame.x, frame.y + frame.height - size, size, size},
        {frame.x + frame.width - size, frame.y + frame.height - size, size, size},
    };
}

void corner_clip_transformer_t::gen_render_instances(
    std::vector<wf::scene::render_instance_uptr>& instances,
    wf::scene::damage_callback push_damage, wf::output_t *shown_on)
{
    instances.push_back(std::make_unique<corner_clip_render_instance_t>(this, push_damage, shown_on));
}

std::string corner_clip_transformer_t::stringify() const
{
    return "corner-clip";
}
}
