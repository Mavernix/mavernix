// The plugin "desktop-aside": slides a screen's desktop sideways to uncover
// what lies under it, as OS X 10.9 slides the desktop away from its
// Notification Center. The desktop is the desktop picture, the icons and the
// windows: Wayfire's background, bottom, workspace and unmanaged layers. The
// top and overlay layers (menu bar, dock, the list being uncovered, menus)
// stay where they are.
//
// IPC method "desktop-aside/move": output (its name), by (in points, negative
// to the left; 0 brings the desktop home), duration_ms and curve ("linear",
// "ease-in", "ease-out" or "ease-in-out": the shell's own curves, so its half
// of the motion keeps pace). The slide starts from wherever the desktop is,
// so a move can turn one around halfway. If the program that moved the
// desktop goes away, the desktop comes home at once.
//
// A press on the desktop while it is aside goes no further. The program that
// moved it gets the event "desktop-aside-pressed" (with the output's name)
// instead, so that it can bring the desktop home, as a click beside
// Notification Center does.
//
// While the desktop is aside, each of its layers' nodes for the screen sits
// inside a translation node: an offset added at drawing time and to pointer
// positions, so the slide costs nothing beyond the frames it draws. Once the
// desktop is home, the nodes go back where they were.

#include <algorithm>
#include <chrono>
#include <cmath>

#include <wayfire/core.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/per-output-plugin.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/scene-operations.hpp>
#include <wayfire/scene.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/unstable/translation-node.hpp>
#include <wayfire/util.hpp>

namespace desktop_aside
{
const std::string MOVE_METHOD = "desktop-aside/move";
const std::string PRESSED_EVENT = "desktop-aside-pressed";

// What slides, back to front.
const wf::scene::layer DESKTOP_LAYERS[] = {
    wf::scene::layer::BACKGROUND,
    wf::scene::layer::BOTTOM,
    wf::scene::layer::WORKSPACE,
    wf::scene::layer::UNMANAGED,
};

/** Progress t (0 to 1) along a curve, the way Qt's quadratic easings draw
 *  the shell's curves (src/shell/ThemeValues.js). */
static double along_curve(const std::string& curve, double t)
{
    if (curve == "linear")
    {
        return t;
    }
    if (curve == "ease-in")
    {
        return t * t;
    }
    if (curve == "ease-out")
    {
        return t * (2 - t);
    }
    // "ease-in-out", which the shell also falls back to.
    return (t < 0.5) ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t);
}

/** Puts `replacement` in `node`'s place among its parent's children. */
static void replace_child(wf::scene::node_ptr node, wf::scene::node_ptr replacement)
{
    auto parent = dynamic_cast<wf::scene::floating_inner_node_t*>(node->parent());
    if (parent == nullptr)
    {
        return;
    }

    auto children = parent->get_children();
    std::replace(children.begin(), children.end(), node, replacement);
    parent->set_children_list(children);
    wf::scene::update(parent->shared_from_this(), wf::scene::update_flag::CHILDREN_LIST);
}

class screen_t : public wf::per_output_plugin_instance_t
{
    // One for each of DESKTOP_LAYERS while the desktop is aside; none at home.
    std::vector<std::shared_ptr<wf::scene::translation_node_t>> shifts;

    // The slide: from where, to where, how, and where it is now.
    double start_offset = 0;
    double end_offset = 0;
    double offset = 0;
    std::string curve = "linear";
    std::chrono::milliseconds duration{0};
    std::chrono::steady_clock::time_point started;
    bool sliding = false;

    // Changing the scene while a frame is drawn is not safe; the layers go
    // back once the frame that brought them home is out.
    wf::wl_idle_call come_home_soon;

    void take_aside()
    {
        for (auto layer : DESKTOP_LAYERS)
        {
            auto layer_node = output->node_for_layer(layer);
            auto shift = std::make_shared<wf::scene::translation_node_t>();
            replace_child(layer_node, shift);
            shift->set_children_list({layer_node});
            wf::scene::update(shift, wf::scene::update_flag::CHILDREN_LIST);
            shifts.push_back(shift);
        }
    }

    void come_home()
    {
        for (auto& shift : shifts)
        {
            // Wayfire may have taken the layer's node out meanwhile, as it
            // does with a screen that is unplugged.
            auto children = shift->get_children();
            shift->set_children_list({});
            if (children.empty())
            {
                wf::scene::remove_child(shift);
            } else
            {
                replace_child(shift, children.front());
            }
        }
        shifts.clear();
    }

    void place(double new_offset)
    {
        offset = new_offset;
        for (auto& shift : shifts)
        {
            // Whole points, so that text stays sharp.
            shift->set_offset({std::round(offset), 0});
            // Wayfire only works out what is visible after a geometry
            // change; a window thought hidden would stay unpainted.
            wf::scene::update(shift, wf::scene::update_flag::GEOMETRY);
        }
        output->render->damage_whole();
    }

    wf::effect_hook_t next_frame = [=]()
    {
        const auto elapsed = std::chrono::steady_clock::now() - started;
        const double progress = (duration.count() > 0) ?
            std::min(1.0, std::chrono::duration<double>(elapsed) / duration) : 1.0;
        place(start_offset + (end_offset - start_offset) * along_curve(curve, progress));

        if (progress < 1.0)
        {
            output->render->schedule_redraw();
            return;
        }

        stop_sliding();
        if (end_offset == 0)
        {
            come_home_soon.run_once([=]() { come_home(); });
        }
    };

    void stop_sliding()
    {
        if (sliding)
        {
            output->render->rem_effect(&next_frame);
            sliding = false;
        }
    }

  public:
    // The program whose move left the desktop aside, if it is.
    wf::ipc::client_interface_t* mover = nullptr;

    /** Whether the node is part of this desktop while it is aside or on its
     *  way there (not on its way home). */
    bool holds(wf::scene::node_t* node) const
    {
        if (shifts.empty() || (end_offset == 0))
        {
            return false;
        }
        for (; node != nullptr; node = node->parent())
        {
            for (auto& shift : shifts)
            {
                if (node == shift.get())
                {
                    return true;
                }
            }
        }
        return false;
    }

    void move(double by, int duration_ms, const std::string& new_curve)
    {
        come_home_soon.disconnect();
        if (shifts.empty())
        {
            if (by == 0)
            {
                return;
            }
            take_aside();
        }

        start_offset = offset;
        end_offset = by;
        curve = new_curve;
        duration = std::chrono::milliseconds(std::max(0, duration_ms));
        started = std::chrono::steady_clock::now();
        if (!sliding)
        {
            output->render->add_effect(&next_frame, wf::OUTPUT_EFFECT_PRE);
            sliding = true;
        }
        output->render->schedule_redraw();
    }

    void fini() override
    {
        stop_sliding();
        come_home_soon.disconnect();
        place(0);
        come_home();
    }
};

class plugin_t : public wf::per_output_plugin_t<screen_t>
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repo;

    wf::ipc::method_callback_full on_move = [=](const wf::json_t& data, wf::ipc::client_interface_t* client)
    {
        const std::string name = wf::ipc::json_get_string(data, "output");
        auto output = wf::get_core().output_layout->find_output(name);
        if ((output == nullptr) || !output_instance.count(output))
        {
            return wf::ipc::json_error("there is no screen called " + name);
        }

        const double by = double(wf::ipc::json_get_int64(data, "by"));
        const int duration_ms = int(wf::ipc::json_get_optional_int64(data, "duration_ms").value_or(0));
        const std::string curve = wf::ipc::json_get_optional_string(data, "curve").value_or("ease-in-out");

        auto& screen = output_instance[output];
        screen->move(by, duration_ms, curve);
        screen->mover = (by == 0) ? nullptr : client;
        return wf::ipc::json_ok();
    };

    wf::signal::connection_t<wf::ipc::client_disconnected_signal> on_client_disconnected =
        [=](wf::ipc::client_disconnected_signal *event)
    {
        for (auto& [output, screen] : output_instance)
        {
            if (screen->mover == event->client)
            {
                screen->move(0, 0, "linear");
                screen->mover = nullptr;
            }
        }
    };

    // A press that was kept from the desktop, whose release must be kept
    // from it too; 0 for none.
    uint32_t kept_button = 0;

    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_button_event>> on_button =
        [=](wf::input_event_signal<wlr_pointer_button_event> *signal)
    {
        const auto *event = signal->event;
        if (event->state == WL_POINTER_BUTTON_STATE_RELEASED)
        {
            if ((kept_button != 0) && (event->button == kept_button))
            {
                signal->mode = wf::input_event_processing_mode_t::IGNORE;
                kept_button = 0;
            }
            return;
        }

        const auto under = wf::get_core().scene()->find_node_at(wf::get_core().get_cursor_position());
        if (!under.has_value())
        {
            return;
        }
        for (auto& [output, screen] : output_instance)
        {
            if ((screen->mover != nullptr) && screen->holds(under->node.get()))
            {
                signal->mode = wf::input_event_processing_mode_t::IGNORE;
                kept_button = event->button;

                wf::json_t message;
                message["event"] = PRESSED_EVENT;
                message["output"] = output->to_string();
                screen->mover->send_json(std::move(message));
                return;
            }
        }
    };

  public:
    void init() override
    {
        per_output_plugin_t::init();
        repo->register_method(MOVE_METHOD, on_move);
        repo->connect(&on_client_disconnected);
        wf::get_core().connect(&on_button);
    }

    void fini() override
    {
        on_button.disconnect();
        on_client_disconnected.disconnect();
        repo->unregister_method(MOVE_METHOD);
        per_output_plugin_t::fini();
    }
};
} // namespace desktop_aside

DECLARE_WAYFIRE_PLUGIN(desktop_aside::plugin_t);
