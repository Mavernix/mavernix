// The plugin "shared-edges": dragging the edge two tiled windows share
// makes one grow as the other shrinks. Wayfire's own resize un-tiles the one
// window it holds.
//
// Every resize request goes through `wf::get_core().default_wm`; this plugin
// replaces it with one that takes single-edge drags on tiled windows with
// tiled neighbours and leaves the rest to the stock code. Each movement
// resizes all windows on the seam in one transaction, so no gap shows.

#include <linux/input-event-codes.h>

#include <vector>

#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/output.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/input-grab.hpp>
#include <wayfire/scene-input.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/toplevel.hpp>
#include <wayfire/txn/transaction-manager.hpp>
#include <wayfire/txn/transaction.hpp>
#include <wayfire/window-manager.hpp>
#include <wayfire/workspace-set.hpp>
#include <wlr/util/edges.h>

#include "seam-math.hpp"

namespace
{
const std::string PLUGIN_NAME = "shared-edges";

// The stock window manager, but asking this plugin first about resizes.
class shared_edge_window_manager_t : public wf::window_manager_t
{
  public:
    std::function<bool(wayfire_toplevel_view, uint32_t)> take_over_resize;

    void resize_request(wayfire_toplevel_view view, uint32_t edges = 0) override
    {
        if (take_over_resize && take_over_resize(view, edges))
        {
            return;
        }

        wf::window_manager_t::resize_request(view, edges);
    }
};

// Nothing for a corner.
std::optional<shared_edges::side_t> single_side(uint32_t edges)
{
    switch (edges)
    {
    case WLR_EDGE_LEFT:
        return shared_edges::side_t::LEFT;

    case WLR_EDGE_RIGHT:
        return shared_edges::side_t::RIGHT;

    case WLR_EDGE_TOP:
        return shared_edges::side_t::TOP;

    case WLR_EDGE_BOTTOM:
        return shared_edges::side_t::BOTTOM;

    default:
        return std::nullopt;
    }
}

bool is_tiled_and_resizable(wayfire_toplevel_view view)
{
    return view->is_mapped() && !view->minimized && !view->pending_fullscreen() &&
           (view->role == wf::VIEW_ROLE_TOPLEVEL) && view->pending_tiled_edges() &&
           (view->get_allowed_actions() & wf::VIEW_ALLOW_RESIZE);
}

shared_edges::window_t describe(wayfire_toplevel_view view)
{
    // Size limits are of the contents; add the frame as the stock plugin does.
    const auto& margins = view->toplevel()->pending().margins;
    const auto frame_width = static_cast<int>(margins.left + margins.right);
    const auto frame_height = static_cast<int>(margins.top + margins.bottom);
    const wf::dimensions_t least = view->toplevel()->get_min_size();
    const wf::dimensions_t largest = view->toplevel()->get_max_size();

    const auto geometry = view->toplevel()->pending().geometry;
    shared_edges::window_t window;
    window.box = {int(geometry.x), int(geometry.y), int(geometry.width), int(geometry.height)};
    window.min_width = std::max(1, least.width) + frame_width;
    window.min_height = std::max(1, least.height) + frame_height;
    window.max_width = largest.width > 0 ? largest.width + frame_width : 0;
    window.max_height = largest.height > 0 ? largest.height + frame_height : 0;
    return window;
}

class shared_edges_t : public wf::plugin_interface_t, public wf::pointer_interaction_t
{
    wf::option_wrapper_t<bool> enabled{PLUGIN_NAME + "/enabled"};
    wf::option_wrapper_t<int> tolerance_px{PLUGIN_NAME + "/tolerance"};
    wf::option_wrapper_t<int> least_size_px{PLUGIN_NAME + "/min_size"};

    wf::plugin_activation_data_t grab_interface = {
        .name = PLUGIN_NAME,
        .capabilities = wf::CAPABILITY_GRAB_INPUT | wf::CAPABILITY_MANAGE_DESKTOP,
    };

    // The drag under way. Every movement is worked out from `starting`, the
    // windows as they were when the drag began, so that it does not drift.
    wf::output_t* output = nullptr;
    std::unique_ptr<wf::input_grab_t> input_grab;
    std::vector<wayfire_toplevel_view> members;
    std::vector<shared_edges::window_t> starting;
    shared_edges::seam_t seam;
    wf::pointf_t pointer_at_start;
    int last_move_px = 0;

    bool dragging() const { return output != nullptr; }

    wf::signal::connection_t<wf::view_disappeared_signal> on_view_disappeared =
        [=](wf::view_disappeared_signal* event) {
        for (auto& member : members)
        {
            if (member == event->view)
            {
                end_drag();
                return;
            }
        }
    };

    // Returns whether this plugin takes the drag.
    bool take_over(wayfire_toplevel_view view, uint32_t edges)
    {
        const auto side = single_side(edges);
        if (!enabled || dragging() || !side || !view->get_output() || !is_tiled_and_resizable(view))
        {
            return false;
        }

        wf::output_t* view_output = view->get_output();
        std::vector<wayfire_toplevel_view> candidates;
        std::vector<shared_edges::window_t> windows;
        std::size_t dragged = 0;
        const uint32_t which = wf::WSET_MAPPED_ONLY | wf::WSET_EXCLUDE_MINIMIZED | wf::WSET_CURRENT_WORKSPACE;
        for (auto& other : view_output->wset()->get_views(which))
        {
            if (other == view)
            {
                dragged = candidates.size();
            }
            else if (!is_tiled_and_resizable(other))
            {
                continue;
            }

            candidates.push_back(other);
            windows.push_back(describe(other));
        }

        auto found = shared_edges::find_seam(windows, dragged, *side, tolerance_px);
        if (!found || !view_output->activate_plugin(&grab_interface))
        {
            return false;
        }

        // Keep only the seam's windows and renumber its indices to match.
        output = view_output;
        seam = {found->vertical, {}, {}};
        for (std::size_t old_index : found->before)
        {
            seam.before.push_back(members.size());
            members.push_back(candidates[old_index]);
            starting.push_back(windows[old_index]);
        }

        for (std::size_t old_index : found->after)
        {
            seam.after.push_back(members.size());
            members.push_back(candidates[old_index]);
            starting.push_back(windows[old_index]);
        }

        pointer_at_start = wf::get_core().get_cursor_position();
        last_move_px = 0;
        output->connect(&on_view_disappeared);

        input_grab = std::make_unique<wf::input_grab_t>(PLUGIN_NAME, output, nullptr, this, nullptr);
        input_grab->set_wants_raw_input(true);
        input_grab->grab_input(wf::scene::layer::OVERLAY);
        grab_interface.cancel = [=]() { end_drag(); };

        wf::get_core().set_cursor(wlr_xcursor_get_resize_name((wlr_edges)edges));
        return true;
    }

    void end_drag()
    {
        if (!dragging())
        {
            return;
        }

        on_view_disappeared.disconnect();
        input_grab->ungrab_input();
        input_grab.reset();
        output->deactivate_plugin(&grab_interface);
        output = nullptr;
        members.clear();
        starting.clear();
    }

    void follow_pointer()
    {
        const wf::pointf_t pointer = wf::get_core().get_cursor_position();
        const double travelled = seam.vertical ? pointer.x - pointer_at_start.x : pointer.y - pointer_at_start.y;
        const int move_px = shared_edges::limit_move(starting, seam, int(travelled), least_size_px);
        if (move_px == last_move_px)
        {
            return;
        }

        last_move_px = move_px;
        auto transaction = wf::txn::transaction_t::create();
        for (std::size_t index = 0; index < members.size(); index++)
        {
            const shared_edges::box_t box = shared_edges::moved_box(starting[index], seam, index, move_px);
            auto& pending = members[index]->toplevel()->pending();
            pending.geometry = wf::geometryf_t{double(box.x), double(box.y), double(box.width), double(box.height)};
            // Gravity: the edge that stays put if the client picks another size.
            const bool is_before = std::find(seam.before.begin(), seam.before.end(), index) != seam.before.end();
            if (seam.vertical)
            {
                pending.gravity = is_before ? WLR_EDGE_LEFT : WLR_EDGE_RIGHT;
            }
            else
            {
                pending.gravity = is_before ? WLR_EDGE_TOP : WLR_EDGE_BOTTOM;
            }

            transaction->add_object(members[index]->toplevel());
        }

        wf::get_core().tx_manager->schedule_transaction(std::move(transaction));
    }

  public:
    void init() override
    {
        auto manager = std::make_unique<shared_edge_window_manager_t>();
        manager->take_over_resize = [=](wayfire_toplevel_view view, uint32_t edges) { return take_over(view, edges); };
        wf::get_core().default_wm = std::move(manager);
    }

    void fini() override
    {
        end_drag();
        wf::get_core().default_wm = std::make_unique<wf::window_manager_t>();
    }

    void handle_pointer_button(const wlr_pointer_button_event& event) override
    {
        if ((event.state == WL_POINTER_BUTTON_STATE_RELEASED) && (event.button == BTN_LEFT))
        {
            end_drag();
        }
    }

    void handle_pointer_motion(wf::pointf_t, uint32_t) override
    {
        if (dragging())
        {
            follow_pointer();
        }
    }
};
} // namespace

DECLARE_WAYFIRE_PLUGIN(shared_edges_t);
