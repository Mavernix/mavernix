// Paint perspective window reflections between the dock glass and its icons.
// The dock supplies its actual geometry; no pixels are read back to the CPU.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <wayfire/scene-operations.hpp>
#include <map>
#include <memory>
#include <wayfire/core.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/output.hpp>
#include <wayfire/per-output-plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/scene-render.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/workspace-set.hpp>

namespace dock_reflections
{
struct picture_t
{
    uint32_t id = 0;
    wf::auxilliary_buffer_t buffer;
    std::unique_ptr<wf::auxilliary_buffer_t> filtered;
    wf::geometry_t box;
};
struct state_t
{
    wf::option_wrapper_t<double> blur{"dock-window-reflections/blur"};
    wf::geometry_t shelf{0, 0, 0, 0};
    double opacity = .35, compression = .32, slant = .6;
    int face = 4;
    bool active = false;
    std::vector<std::unique_ptr<picture_t>> pictures;
    uint64_t frames = 0;
    double capture_ms = 0;
};
static const char* vertex = R"(#version 100
attribute vec2 position;
uniform mat4 mvp;
varying highp vec2 point;
void main() { point=position; gl_Position=mvp*vec4(position,0.0,1.0); }
)";
static const char* fragment = R"(#version 100
@builtin_ext@
precision highp float;
@builtin@
uniform vec4 window_box;
uniform vec4 shelf;
uniform float strength;
uniform float compression;
uniform float slant;
uniform float face;
uniform vec2 texel;
varying highp vec2 point;
vec4 sampleWindow(vec2 uv) {
 if (any(lessThan(uv,vec2(0.0))) || any(greaterThan(uv,vec2(1.0)))) return vec4(0.0);
 return get_pixel(vec2(uv.x,1.0-uv.y));
}
void main() {
 float distance=point.y-shelf.y;
 float surfaceHeight=max(1.0,shelf.w-face);
 float depth=clamp(distance/surfaceHeight,0.0,1.0);
 float inset=(1.0-depth)*surfaceHeight*slant;
 if ( point.x<shelf.x+inset || point.x>shelf.x+shelf.z-inset) discard;
 // Project the nearest window edge onto the rear of the shelf.
 float origin=min(shelf.y,window_box.y+window_box.w);
 float proximity=1.0-smoothstep(0.0,shelf.w/compression,shelf.y-origin);
 // Inverse perspective projection onto the glass plane. Its rear edge is the anchor.
 float inverseScale=1.0/(1.0+0.25*depth);
 vec2 original=vec2(point.x,
                    origin-min(distance,surfaceHeight)*inverseScale/compression);
 vec2 uv=(original-window_box.xy)/window_box.zw;
 // A Gaussian kernel in reflected screen pixels, with transparent window edges.
 // Symmetric pairs on a golden-angle disk give a stationary, smooth blur.
 vec4 pixel=vec4(0.0);
 float total=0.0;
 vec2 direction=vec2(1.0,0.0);
 for (int i=0;i<32;++i) {
   float radiusSquared=(float(i)+0.5)/8.0;
   vec2 offset=direction*sqrt(radiusSquared)*texel;
   float weight=exp(-0.5*radiusSquared);
   pixel+=(sampleWindow(uv+offset)+sampleWindow(uv-offset))*weight;
   total+=2.0*weight;
   direction=mat2(-0.73736888,0.67549029,-0.67549029,-0.73736888)*direction;
 }
 pixel/=total;
 float light=dot(pixel.rgb,vec3(0.2126,0.7152,0.0722))/max(pixel.a,0.001);
 float dark=1.0-smoothstep(0.015,0.42,light);
 // The silver shelf lifts the darkest reflections toward its own neutral tone.
 pixel.rgb=mix(pixel.rgb,vec3(0.28)*pixel.a,0.65*dark);
 float ink=1.0-0.65*dark;
 float fade=strength*proximity*mix(1.0,0.20,smoothstep(0.0,1.0,depth))*ink;
 gl_FragColor=pixel*fade;
}
)";
class node_t : public wf::scene::transformer_base_node_t
{
  public:
    std::shared_ptr<state_t> state;
    OpenGL::program_t program;
    node_t(std::shared_ptr<state_t> s) : transformer_base_node_t(false), state(s)
    {
        wf::gles::run_in_context_if_gles([&] { program.compile(vertex, fragment); });
    }
    ~node_t()
    {
        wf::gles::run_in_context_if_gles([&] { program.free_resources(); });
    }
    class instance_t : public wf::scene::transformer_render_instance_t<node_t>
    {
      public:
        wf::signal::connection_t<wf::scene::node_damage_signal> changed = [this](auto* event) {
            this->_push_damage(event->region);
        };
        instance_t(node_t* self, wf::scene::damage_callback damage, wf::output_t* output)
            : transformer_render_instance_t(self, damage, output)
        {
            self->connect(&changed);
        }
        void schedule_instructions(std::vector<wf::scene::render_instruction_t>& instructions,
            const wf::render_target_t& target, wf::regionf_t& damage) override
        {
            auto repaint = damage & self->get_bounding_box();
            // Instructions execute backwards: paint the shelf/blur first, then its reflection.
            if (!repaint.empty())
                instructions.push_back({.instance = this, .target = target, .damage = repaint});
            for (auto& child : children)
                child->schedule_instructions(instructions, target, damage);
        }
        void render(const wf::scene::render_instruction_t& data) override
        {
            if (data.damage.empty())
                return;
            data.pass->custom_gles_subpass([&] {
                wf::gles::bind_render_buffer(data.target);
                auto& s = *self->state;
                const auto& b = s.shelf;
                int height = std::max(1.0, b.height);
                const float quad[] = {float(b.x), float(b.y), float(b.x + b.width), float(b.y), float(b.x + b.width),
                    float(b.y + height), float(b.x), float(b.y + height)};
                if (s.active)
                    for (auto& p : s.pictures)
                    {
                        auto tex = wf::gles_texture_t::from_aux(p->filtered ? *p->filtered : p->buffer);
                        self->program.use(tex.type);
                        self->program.uniformMatrix4f(
                            "mvp", wf::gles::render_target_orthographic_projection(data.target));
                        self->program.uniform4f(
                            "window_box", glm::vec4(p->box.x, p->box.y, p->box.width, p->box.height));
                        self->program.uniform4f("shelf", glm::vec4(b.x, b.y, b.width, height));
                        self->program.uniform1f("strength", s.opacity);
                        self->program.uniform1f("compression", s.compression);
                        self->program.uniform1f("slant", s.slant);
                        self->program.uniform1f("face", s.face);
                        self->program.uniform2f(
                            "texel", double(s.blur) / p->box.width, double(s.blur) / s.compression / p->box.height);
                        self->program.set_active_texture(tex);
                        self->program.attrib_pointer("position", 2, 0, quad);
                        wf::gles::for_each_scissor_rect(
                            data.target, data.damage & b, [&] { glDrawArrays(GL_TRIANGLE_FAN, 0, 4); });
                        self->program.deactivate();
                    }
                glDisable(GL_SCISSOR_TEST);
                glBindTexture(GL_TEXTURE_2D, 0);
            });
        }
    };
    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& out, wf::scene::damage_callback damage,
        wf::output_t* output) override
    {
        out.push_back(std::make_unique<instance_t>(this, damage, output));
    }
};
class output_t : public wf::per_output_plugin_instance_t
{
  public:
    std::shared_ptr<state_t> state = std::make_shared<state_t>();
    std::weak_ptr<node_t> dock_node;
    wf::effect_hook_t prepare = [this] {
        auto node = dock_node.lock();
        if (!node)
            return;
        if (output->render->get_scheduled_damage().empty())
        {
            auto candidates = output->wset()->get_views(
                wf::WSET_MAPPED_ONLY | wf::WSET_EXCLUDE_MINIMIZED | wf::WSET_CURRENT_WORKSPACE);
            if (std::none_of(candidates.begin(), candidates.end(),
                    [](auto view) { return !view->get_transformed_node()->is_enabled(); }))
                return;
        }
        bool had = !state->pictures.empty();
        state->pictures.clear();
        if (state->active && state->shelf.width > 0)
        {
            auto begin = std::chrono::steady_clock::now();
            auto views = output->wset()->get_views(wf::WSET_MAPPED_ONLY | wf::WSET_EXCLUDE_MINIMIZED |
                                                   wf::WSET_CURRENT_WORKSPACE | wf::WSET_SORT_STACKING);
            std::reverse(views.begin(), views.end());
            // A real interactive drag uses a proxy above the normal scene tree.
            // Its original transform manager is disabled but still renders when explicitly requested.
            std::stable_partition(
                views.begin(), views.end(), [](auto view) { return view->get_transformed_node()->is_enabled(); });
            for (auto view : views)
            {
                auto root = view->get_transformed_node();
                auto base = view->get_surface_root_node()->get_bounding_box();
                auto visual = root->get_bounding_box();
                auto frame = view->get_geometry();
                // Preserve frame insets while following the proxy's live translation/scale.
                // Using only stored window geometry leaves the reflection at the pre-drag position.
                double sx = visual.width / std::max(1.0, base.width), sy = visual.height / std::max(1.0, base.height);
                wf::geometry_t box{visual.x + (frame.x - base.x) * sx, visual.y + (frame.y - base.y) * sy,
                    frame.width * sx, frame.height * sy};
                auto b = state->shelf;
                double padding_x = 2.0 * double(state->blur);
                double padding_y = padding_x / state->compression;
                double reach = (b.height - state->face) / state->compression + padding_y;
                if (box.x + box.width <= b.x - padding_x || box.x >= b.x + b.width + padding_x ||
                    box.y >= b.y + padding_y || box.y + box.height < b.y - reach)
                    continue;
                auto picture = std::make_unique<picture_t>();
                picture->id = view->get_id();
                // Crop to the source strip before rendering: large windows cost only a narrow GPU buffer.
                double left = std::max(box.x, std::floor(b.x - padding_x)),
                       right = std::min(box.x + box.width, std::ceil(b.x + b.width + padding_x));
                double top = std::max(box.y, std::floor(std::min(b.y, box.y + box.height) - reach)),
                       bottom = std::min(box.y + box.height, std::ceil(b.y + padding_y));
                if (right <= left || bottom <= top)
                    continue;
                picture->box = {left, top, right - left, bottom - top};
                float scale = output->handle->scale;
                picture->buffer.allocate(wf::dimensions(picture->box), scale);
                wf::render_target_t target{picture->buffer};
                target.geometry = picture->box;
                target.scale = scale;
                std::vector<wf::scene::render_instance_uptr> instances;
                root->gen_render_instances(instances, [](auto) {}, output);
                wf::render_pass_params_t params;
                params.background_color = {0, 0, 0, 0};
                params.damage = picture->box;
                params.target = target;
                params.instances = &instances;
                params.flags = wf::RPASS_CLEAR_BACKGROUND;
                wf::render_pass_t::run(params);
                auto size = picture->buffer.get_size();
                if (size.width <= 0 || size.height <= 0)
                    continue;
                // Average neighboring source pixels before the compressed projection samples them.
                // Halve each dimension independently so vertical detail does not alias while scrolling.
                int levelsX =
                    std::clamp(int(std::floor(std::log2(std::max(1.0, double(state->blur) * scale / 3)))), 0, 3);
                int levelsY = std::clamp(
                    int(std::floor(std::log2(std::max(1.0, double(state->blur) * scale / (3 * state->compression))))),
                    0, 4);
                wf::gles::run_in_context_if_gles([&] {
                    GLint readFbo, drawFbo;
                    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
                    auto scissor = glIsEnabled(GL_SCISSOR_TEST);
                    glDisable(GL_SCISSOR_TEST);
                    for (int level = 0; level < std::max(levelsX, levelsY); ++level)
                    {
                        auto& previous = picture->filtered ? *picture->filtered : picture->buffer;
                        auto old = previous.get_size();
                        wf::dimensions_t small{level < levelsX ? (old.width + 1) / 2 : old.width,
                            level < levelsY ? (old.height + 1) / 2 : old.height};
                        auto next = std::make_unique<wf::auxilliary_buffer_t>();
                        next->allocate(small);
                        if (!next->get_texture())
                            break;
                        auto from = wf::gles::ensure_render_buffer_fb_id(previous.get_renderbuffer());
                        auto to = wf::gles::ensure_render_buffer_fb_id(next->get_renderbuffer());
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, from);
                        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, to);
                        glBlitFramebuffer(0, 0, old.width, old.height, 0, 0, small.width, small.height,
                            GL_COLOR_BUFFER_BIT, GL_LINEAR);
                        picture->filtered = std::move(next);
                    }
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
                    if (scissor)
                        glEnable(GL_SCISSOR_TEST);
                });
                state->pictures.push_back(std::move(picture));
            }
            state->capture_ms =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
            state->frames++;
        }
        if (had || !state->pictures.empty())
        {
            // Refresh this frame without scheduling another one: idle must stay idle.
            for (auto parent = node->parent(); parent; parent = parent->parent())
                if (auto transform = dynamic_cast<wf::scene::transformer_base_node_t*>(parent))
                    transform->cached_damage |= state->shelf;
            output->render->damage(state->shelf, false);
        }
    };
    void attach(wayfire_view view)
    {
        if (view->get_app_id() != "mavernix-dock-shelf" || view->get_output() != output)
            return;
        auto manager = view->get_transformed_node();
        if (manager->get_transformer<node_t>())
            return;
        auto node = std::make_shared<node_t>(state);
        manager->add_transformer(node, wf::TRANSFORMER_BLUR + 1);
        // Both surfaces share the overlay layer; the icons must remain above the shelf.
        for (auto other : wf::get_core().get_all_views())
        {
            if (other->get_app_id() == "mavernix-dock" && other->is_mapped() && other->get_output() == output)
            {
                auto root = other->get_root_node();
                if (root->parent() == view->get_root_node()->parent())
                    wf::scene::raise_to_front(root);
            }
        }
        dock_node = node;
    }
    void init() override { output->render->add_effect(&prepare, wf::OUTPUT_EFFECT_DAMAGE); }
    void fini() override
    {
        output->render->rem_effect(&prepare);
        for (auto view : wf::get_core().get_all_views())
            if (view->get_output() == output)
                view->get_transformed_node()->rem_transformer<node_t>();
        state->pictures.clear();
    }
};
class plugin_t : public wf::plugin_interface_t, public wf::per_output_tracker_mixin_t<output_t>
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> methods;
    wf::signal::connection_t<wf::view_mapped_signal> mapped = [this](auto* event) {
        for (auto& [output, inst] : output_instance)
            inst->attach(event->view);
    };

  public:
    void init() override
    {
        if (!wf::get_core().is_gles2())
            return;
        init_output_tracking();
        wf::get_core().connect(&mapped);
        for (auto view : wf::get_core().get_all_views())
            for (auto& [output, inst] : output_instance)
                inst->attach(view);
        methods->register_method("dock-window-reflections/configure", [this](const wf::json_t& data) {
            std::string name = wf::ipc::json_get_string(data, "output");
            for (auto& [output, inst] : output_instance)
                if (output->to_string() == name || name.empty())
                {
                    auto& s = *inst->state;
                    int x = wf::ipc::json_get_int64(data, "x"), y = wf::ipc::json_get_int64(data, "y");
                    int width = wf::ipc::json_get_int64(data, "width"),
                        height = wf::ipc::json_get_int64(data, "height");
                    if (width < 0 || height < 0 || width > 32768 || height > 512)
                        return wf::ipc::json_error("Invalid shelf geometry");
                    auto opacity = wf::ipc::json_get_optional_double(data, "opacity").value_or(.35);
                    auto compression = wf::ipc::json_get_optional_double(data, "compression").value_or(.32);
                    auto slant = wf::ipc::json_get_optional_double(data, "slant").value_or(.6);
                    auto face = wf::ipc::json_get_optional_int64(data, "face").value_or(4);
                    if (!std::isfinite(opacity) || opacity < 0 || opacity > 1 || !std::isfinite(compression) ||
                        compression < .05 || compression > 1 || !std::isfinite(slant) || slant < 0 || slant > 4 ||
                        face < 0 || face > 128)
                        return wf::ipc::json_error("Invalid reflection appearance");
                    s.opacity = opacity;
                    s.compression = compression;
                    s.slant = slant;
                    s.face = face;
                    s.shelf = {double(x), double(y), double(width), double(height)};
                    s.active = width > 0 && height > 0;
                    output->render->damage(s.shelf);
                    if (auto node = inst->dock_node.lock())
                        wf::scene::damage_node(node, node->get_bounding_box());
                    return wf::ipc::json_ok();
                }
            return wf::ipc::json_error("Output not found");
        });
        methods->register_method("dock-window-reflections/stats", [this](const wf::json_t&) {
            wf::json_t result = wf::ipc::json_ok();
            for (auto& [output, inst] : output_instance)
            {
                auto& s = *inst->state;
                result["frames"] = s.frames;
                result["pictures"] = s.pictures.size();
                result["capture_ms"] = s.capture_ms;
                result["output"] = output->to_string();
                result["attached"] = !inst->dock_node.expired();
                result["x"] = s.shelf.x;
                result["y"] = s.shelf.y;
                result["width"] = s.shelf.width;
                result["height"] = s.shelf.height;
                result["blur"] = double(s.blur);
                wf::json_t sources;
                for (auto& picture : s.pictures)
                {
                    wf::json_t geometry;
                    geometry["x"] = picture->box.x;
                    geometry["y"] = picture->box.y;
                    geometry["width"] = picture->box.width;
                    geometry["height"] = picture->box.height;
                    sources[std::to_string(picture->id)] = geometry;
                }
                result["sources"] = sources;
            }
            return result;
        });
    }
    void fini() override
    {
        mapped.disconnect();
        methods->unregister_method("dock-window-reflections/configure");
        methods->unregister_method("dock-window-reflections/stats");
        fini_output_tracking();
    }
};
} // namespace dock_reflections
DECLARE_WAYFIRE_PLUGIN(dock_reflections::plugin_t)
