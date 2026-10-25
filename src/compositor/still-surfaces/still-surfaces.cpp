// The plugin "still-surfaces": keeps chosen shell surfaces in place while
// Wayfire's vswitch/vswipe slide the desktops, which would otherwise carry
// every panel and dock along.
//
// For the length of the switch, each surface named in "names" (by its
// layer-shell namespace) is moved from its layer to the front of the scene
// tree, out of the sliding row, and put back afterwards.
//
// Surfaces in "sliding" (a menu bar) slide as two: a snapshot of the old
// contents leaves with the old desktop, and the live surface arrives with
// the new one. A single surface in the row would show the new contents on
// both. vswitch does not publish the row's position, so it is recomputed
// from "vswitch/duration"; under vswipe it cannot be known, and such a
// surface stays in the row.
//
// Assumes one screen at the scene's origin: at the front of the tree,
// positions are global rather than per output.

#include <cmath>
#include <sstream>
#include <vector>

#include <wayfire/core.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/scene-operations.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/render.hpp>
#include <wayfire/scene-render.hpp>
#include <wayfire/scene.hpp>
#include <wayfire/util/duration.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/workspace-set.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/util.hpp>
#include <wayfire/view.hpp>

namespace still_surfaces
{
static std::vector<std::string> words_of(const std::string& text)
{
    std::vector<std::string> words;
    std::istringstream stream(text);
    std::string word;
    while (stream >> word)
    {
        words.push_back(word);
    }
    return words;
}

static bool contains(const std::vector<std::string>& words, const std::string& word)
{
    return std::find(words.begin(), words.end(), word) != words.end();
}

// The plugin whose sliding row's place can be worked out here.
const std::string KEY_SWITCH_PLUGIN = "vswitch";
const std::string SHIFT_NAME = "still-surfaces-shift";

/** A snapshot of a surface, drawn at the surface's place shifted sideways. */
class picture_node_t : public wf::scene::node_t
{
    class render_instance_t : public wf::scene::simple_render_instance_t<picture_node_t>
    {
      public:
        using simple_render_instance_t::simple_render_instance_t;

        void render(const wf::scene::render_instruction_t& data) override
        {
            data.pass->add_texture(self->texture, data.target, self->get_bounding_box(), data.damage);
        }
    };

    wf::auxilliary_buffer_t contents;
    std::shared_ptr<wf::texture_t> texture;
    wf::geometry_t place;
    int shift = 0;

  public:
    /** False if the surface has nothing to show. */
    bool take(wayfire_view view)
    {
        view->take_snapshot(contents);
        const auto size = contents.get_size();
        if ((size.width <= 0) || (size.height <= 0))
        {
            return false;
        }
        texture = wf::texture_t::from_aux(contents);
        place = view->get_surface_root_node()->get_bounding_box();
        return true;
    }

    picture_node_t() : node_t(false) {}

    void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
        wf::scene::damage_callback push_damage, wf::output_t* output) override
    {
        instances.push_back(std::make_unique<render_instance_t>(this, push_damage, output));
    }

    wf::geometry_t get_bounding_box() override { return {place.x + shift, place.y, place.width, place.height}; }

    void set_shift(int new_shift)
    {
        wf::scene::damage_node(this->shared_from_this(), get_bounding_box());
        shift = new_shift;
        wf::scene::damage_node(this->shared_from_this(), get_bounding_box());
    }
};

class plugin_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<std::string> names{"still-surfaces/names"};
    wf::option_wrapper_t<std::string> sliding{"still-surfaces/sliding"};
    wf::option_wrapper_t<std::string> during{"still-surfaces/during"};
    // vswitch's own options, to follow its row.
    wf::option_wrapper_t<int> row_gap{"vswitch/gap"};
    wf::option_wrapper_t<wf::animation_description_t> row_duration{"vswitch/duration"};
    wf::animation::simple_animation_t row_progress{row_duration};

    struct held_t
    {
        wf::scene::floating_inner_ptr surface;
        std::weak_ptr<wf::scene::floating_inner_node_t> layer;
    };
    std::vector<held_t> held;

    struct slid_t
    {
        wayfire_view view;
        std::shared_ptr<wf::scene::view_2d_transformer_t> shift;
        std::shared_ptr<picture_node_t> picture;
    };
    std::vector<slid_t> slid;
    // How far the row travels, in points, positive to the right; zero
    // while nothing slides.
    wf::output_t* sliding_output = nullptr;
    wf::point_t desktop_before = {0, 0};
    double row_travel = 0;

    wf::wl_idle_call hold_soon;

    // Plugins named in "during" active now; a swipe can end in a slide.
    int active_count = 0;

    void hold_one(wayfire_view view)
    {
        const wf::scene::floating_inner_ptr surface = view->get_root_node();
        auto layer = dynamic_cast<wf::scene::floating_inner_node_t*>(surface->parent());
        if (layer == nullptr)
        {
            return;
        }

        held.push_back(
            {surface, std::dynamic_pointer_cast<wf::scene::floating_inner_node_t>(layer->shared_from_this())});
        wf::scene::readd_front(wf::get_core().scene(), surface);
    }

    void hold()
    {
        const auto wanted = words_of(names);
        for (auto& view : wf::get_core().get_all_views())
        {
            if (view->is_mapped() && contains(wanted, view->get_app_id()))
            {
                hold_one(view);
            }
        }
    }

    // Must run when the switch is announced: a moment later the shell may
    // already show the new desktop's contents.
    void take_pictures(wf::output_t* output)
    {
        sliding_output = output;
        desktop_before = output->wset()->get_current_workspace();

        const auto wanted = words_of(sliding);
        for (auto& view : wf::get_core().get_all_views())
        {
            if (!view->is_mapped() || (view->get_output() != output) || !contains(wanted, view->get_app_id()))
            {
                continue;
            }

            auto picture = std::make_shared<picture_node_t>();
            if (picture->take(view))
            {
                slid.push_back({view, nullptr, picture});
            }
        }
    }

    void start_sliding()
    {
        if (sliding_output == nullptr)
        {
            return;
        }

        const wf::point_t desktop_after = sliding_output->wset()->get_current_workspace();
        const int screen_width = sliding_output->get_layout_geometry().width;
        row_travel = double(desktop_after.x - desktop_before.x) * (screen_width + row_gap);
        if (row_travel == 0)
        {
            slid.clear();
            return;
        }

        for (auto& entry : slid)
        {
            hold_one(entry.view);
            entry.shift = std::make_shared<wf::scene::view_2d_transformer_t>(entry.view);
            entry.view->get_transformed_node()->add_transformer(entry.shift, wf::TRANSFORMER_2D, SHIFT_NAME);
            wf::scene::add_front(wf::get_core().scene(), entry.picture);
        }

        row_progress.animate(0, 1);
        // After each frame, as vswitch moves its row; before the frame it
        // would run one frame ahead of the row.
        sliding_output->render->add_effect(&move_with_row, wf::OUTPUT_EFFECT_POST);
        move_with_row();
    }

    wf::effect_hook_t move_with_row = [=]() {
        const double travelled = row_travel * row_progress;
        for (auto& entry : slid)
        {
            entry.picture->set_shift(int(std::round(-travelled)));
            entry.view->damage();
            entry.shift->translation_x = float(std::round(row_travel - travelled));
            entry.view->damage();
            // Visibility is only recomputed on a geometry update; a surface
            // thought off screen would not repaint until it landed.
            wf::scene::update(entry.view->get_root_node(), wf::scene::update_flag::GEOMETRY);
        }
    };

    void stop_sliding()
    {
        if (sliding_output == nullptr)
        {
            return;
        }

        if (row_travel != 0)
        {
            sliding_output->render->rem_effect(&move_with_row);
        }
        for (auto& entry : slid)
        {
            if (entry.shift)
            {
                entry.view->damage();
                entry.view->get_transformed_node()->rem_transformer(entry.shift);
                entry.view->damage();
                wf::scene::remove_child(entry.picture);
            }
        }
        slid.clear();
        sliding_output = nullptr;
        row_travel = 0;
    }

    void release()
    {
        for (auto& entry : held)
        {
            const auto layer = entry.layer.lock();
            // A surface that went away meanwhile was removed by its owner.
            if (layer && (entry.surface->parent() == wf::get_core().scene().get()))
            {
                wf::scene::readd_front(layer, entry.surface);
            }
        }
        held.clear();
    }

    wf::signal::connection_t<wf::output_plugin_activated_changed_signal> on_plugin_activity =
        [=](wf::output_plugin_activated_changed_signal* signal) {
        if (!contains(words_of(during), signal->plugin_name))
        {
            return;
        }

        if (signal->activated)
        {
            if ((active_count == 0) && (signal->plugin_name == KEY_SWITCH_PLUGIN) && (signal->output != nullptr))
            {
                take_pictures(signal->output);
            }
            if (active_count == 0)
            {
                // Not at once: the signal comes before the plugin puts its
                // row at the tree's front, which would then cover us.
                hold_soon.run_once([=]() {
                    if (active_count > 0)
                    {
                        hold();
                        start_sliding();
                    }
                });
            }
            active_count += 1;
        }
        else if (active_count > 0)
        {
            active_count -= 1;
            if (active_count == 0)
            {
                stop_sliding();
                release();
            }
        }
    };

  public:
    void init() override { wf::get_core().connect(&on_plugin_activity); }

    void fini() override
    {
        on_plugin_activity.disconnect();
        stop_sliding();
        release();
    }
};
} // namespace still_surfaces

DECLARE_WAYFIRE_PLUGIN(still_surfaces::plugin_t);
