#include "warp-transformer.hpp"

#include <cstring>

#include <wayfire/output.hpp>
#include <wayfire/scene-render.hpp>
#include <wayfire/toplevel-view.hpp>

namespace themed_minimize
{
namespace
{
// Distance between mesh rows, in logical pixels; close enough to look curved.
const double MESH_ROW_STEP = 4.0;

// With no icon, a window goes to a spot this size at the bottom middle.
const double FALLBACK_ICON_WIDTH  = 64;
const double FALLBACK_ICON_HEIGHT = 64;

const char *vertex_shader =
    R"(
#version 100

attribute highp vec2 position;
attribute highp vec2 picture_position_in;

uniform mat4 matrix;

varying highp vec2 picture_position;

void main() {
    picture_position = picture_position_in;
    gl_Position = matrix * vec4(position, 0.0, 1.0);
}
)";

// Wayfire replaces @builtin_ext@ and @builtin@ with get_pixel(), which reads
// any kind of window texture.
const char *plain_fragment_shader =
    R"(
#version 100
@builtin_ext@
@builtin@

precision highp float;

varying highp vec2 picture_position;

void main()
{
    gl_FragColor = get_pixel(picture_position);
}
)";

// Used where the card supports derivatives: each drawn point is the mean of
// the stretch of picture it covers (dFdx, dFdy), so a window squeezed to a
// tenth of its width does not break into flickering blocks. Mipmaps would be
// the usual cure, but Wayfire's window textures cannot have them. The window
// is squeezed more sideways than down, so more places are read across.
const char *averaging_fragment_shader =
    R"(
#version 100
#extension GL_OES_standard_derivatives : enable
@builtin_ext@
@builtin@

precision highp float;

varying highp vec2 picture_position;

const int PLACES_ACROSS = 12;
const int PLACES_DOWN = 4;

void main()
{
    vec2 across = dFdx(picture_position);
    vec2 down = dFdy(picture_position);

    vec4 sum = vec4(0.0);
    for (int i = 0; i < PLACES_ACROSS; i++)
    {
        float share_across = (float(i) + 0.5) / float(PLACES_ACROSS) - 0.5;
        for (int j = 0; j < PLACES_DOWN; j++)
        {
            float share_down = (float(j) + 0.5) / float(PLACES_DOWN) - 0.5;
            sum += get_pixel(picture_position + share_across * across + share_down * down);
        }
    }

    gl_FragColor = sum / float(PLACES_ACROSS * PLACES_DOWN);
}
)";

const char *DERIVATIVES_EXTENSION = "GL_OES_standard_derivatives";

wf::geometry_t union_of(wf::geometry_t a, wf::geometry_t b)
{
    const double left   = std::min(a.x, b.x);
    const double top    = std::min(a.y, b.y);
    const double right  = std::max(a.x + a.width, b.x + b.width);
    const double bottom = std::max(a.y + a.height, b.y + b.height);
    return {left, top, right - left, bottom - top};
}

class warp_render_instance_t :
    public wf::scene::transformer_render_instance_t<warp_transformer_t>
{
    wf::signal::connection_t<wf::scene::node_damage_signal> on_node_damaged =
        [=] (wf::scene::node_damage_signal *ev)
    {
        push_to_parent(ev->region);
    };

    wf::scene::damage_callback push_to_parent;

  public:
    warp_render_instance_t(warp_transformer_t *self, wf::scene::damage_callback push_damage,
        wf::output_t *output) :
        wf::scene::transformer_render_instance_t<warp_transformer_t>(self, push_damage, output)
    {
        this->push_to_parent = push_damage;
        self->connect(&on_node_damaged);
    }

    void schedule_instructions(std::vector<wf::scene::render_instruction_t>& instructions,
        const wf::render_target_t& target, wf::regionf_t& damage) override
    {
        instructions.push_back(wf::scene::render_instruction_t{
                    .instance = this,
                    .target   = target,
                    .damage   = damage & self->get_bounding_box(),
                });
    }

    void transform_damage_region(wf::regionf_t& damage) override
    {
        damage |= self->affected_area;
    }

    void render(const wf::scene::render_instruction_t& data) override
    {
        // Only the window's frame flows into the icon; a shadow beyond it is
        // left out.
        const wf::geometry_t picture_area = self->get_children_bounding_box();
        const wf::geometry_t window = self->window_frame(picture_area);
        const std::vector<warp::row_t> rows = self->current_rows(window);
        if (rows.size() < 2)
        {
            return;
        }

        const float frame_left  = (window.x - picture_area.x) / picture_area.width;
        const float frame_right = (window.x + window.width - picture_area.x) / picture_area.width;
        auto share_of_picture_height = [&] (double share_of_frame_height)
        {
            const double y = window.y + share_of_frame_height * window.height;
            return (float)((y - picture_area.y) / picture_area.height);
        };

        // Two points per row, left and right end, drawn as a triangle strip.
        std::vector<float> positions;
        std::vector<float> picture_positions;
        for (const warp::row_t& row : rows)
        {
            // In a texture, height 0 is the bottom edge; in the geometry
            // it is the top.
            const float texture_height = 1.0f - share_of_picture_height(row.picture_height);

            positions.insert(positions.end(), {(float)row.left_x, (float)row.y});
            picture_positions.insert(picture_positions.end(), {frame_left, texture_height});

            positions.insert(positions.end(), {(float)row.right_x, (float)row.y});
            picture_positions.insert(picture_positions.end(), {frame_right, texture_height});
        }

        auto picture = wf::gles_texture_t{this->get_texture(data.target.scale)};
        data.pass->custom_gles_subpass(data.target, [&]
        {
            self->program.use(wf::TEXTURE_TYPE_RGBA);
            self->program.uniformMatrix4f("matrix",
                wf::gles::render_target_orthographic_projection(data.target));
            self->program.attrib_pointer("position", 2, 0, positions.data());
            self->program.attrib_pointer("picture_position_in", 2, 0, picture_positions.data());
            self->program.set_active_texture(picture);
            wf::gles::for_each_scissor_rect(data.target, data.damage, [&]
            {
                GL_CALL(glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 * rows.size()));
            });
        });
    }
};
}

warp_transformer_t::warp_transformer_t(wayfire_view view, effect_t effect,
    wf::animation_description_t duration, wf::geometry_t icon) :
    wf::scene::view_2d_transformer_t(view), _view(view->weak_from_this()), effect(effect), icon(icon),
    progress(wf::create_option<wf::animation_description_t>(duration))
{
    const bool icon_is_known = (icon.width > 0) && (icon.height > 0);
    if (!icon_is_known)
    {
        if (auto output = view->get_output())
        {
            const wf::geometry_t screen = output->get_relative_geometry();
            this->icon = {
                screen.width / 2 - FALLBACK_ICON_WIDTH / 2,
                screen.height - FALLBACK_ICON_HEIGHT,
                FALLBACK_ICON_WIDTH,
                FALLBACK_ICON_HEIGHT,
            };
        }
    }

    affected_area = union_of(view->get_transformed_node()->get_children_bounding_box(), this->icon);

    // With a renderer other than GLES, the window is not drawn during the
    // effect.
    wf::gles::run_in_context_if_gles([&]
    {
        const char *abilities = (const char*)glGetString(GL_EXTENSIONS);
        const bool can_average = abilities && std::strstr(abilities, DERIVATIVES_EXTENSION);
        program.compile(vertex_shader, can_average ? averaging_fragment_shader : plain_fragment_shader);
    });
}

warp_transformer_t::~warp_transformer_t()
{
    wf::gles::run_in_context_if_gles([&]
    {
        program.free_resources();
    });
}

void warp_transformer_t::start(bool hiding)
{
    if (hiding)
    {
        progress.animate(0.0, 1.0);
    } else
    {
        progress.animate(1.0, 0.0);
    }
}

void warp_transformer_t::reverse()
{
    const double now    = progress;
    const double target = (progress.end > 0.5) ? 0.0 : 1.0;
    progress.animate(now, target);
}

bool warp_transformer_t::is_running()
{
    return progress.running();
}

wf::geometry_t warp_transformer_t::window_frame(wf::geometry_t picture_area)
{
    if (auto toplevel = wf::toplevel_cast(_view.lock()))
    {
        return toplevel->get_geometry();
    }

    return picture_area;
}

std::vector<warp::row_t> warp_transformer_t::current_rows(wf::geometry_t window)
{
    affected_area = union_of(window, icon);

    const warp::box_t window_box{window.x, window.y, window.width, window.height};
    const warp::box_t icon_box{icon.x, icon.y, icon.width, icon.height};

    if (effect == effect_t::SCALE)
    {
        return warp::scale_rows(window_box, icon_box, progress);
    }

    return warp::genie_rows(window_box, icon_box, progress, MESH_ROW_STEP);
}

wf::geometry_t warp_transformer_t::get_bounding_box()
{
    return affected_area;
}

void warp_transformer_t::gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
    wf::scene::damage_callback push_damage, wf::output_t *shown_on)
{
    instances.push_back(std::make_unique<warp_render_instance_t>(this, push_damage, shown_on));
}
}
