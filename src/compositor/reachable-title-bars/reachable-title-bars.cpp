// The plugin "reachable-title-bars": no window's title bar ends up under the
// menu bar, where it cannot be dragged. As on OS X, a window's top stays at
// or below the top of the workarea: when the window opens, when a move or
// resize of it ends, and when the workarea gains a menu bar. A window taller
// than the workarea keeps its top there and hangs off the bottom.
//
// The last is how windows get there: across sleep the screen goes away, and
// Wayfire keeps its windows on a 1280x720 stand-in and brings them back to
// the screen before the shell has put the menu bar on it again, so a tall
// window comes back at the very top. Wayfire itself moves only tiled windows
// when the workarea changes.

#include <algorithm>
#include <cmath>

#include <wayfire/core.hpp>
#include <wayfire/output.hpp>
#include <wayfire/per-output-plugin.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/workarea.hpp>
#include <wayfire/workspace-set.hpp>

namespace reachable_title_bars
{
// Full-screen and tiled windows are fitted to the screen already.
static void keep_one_reachable(wayfire_toplevel_view view)
{
    if (!view->get_output() || (view->role != wf::VIEW_ROLE_TOPLEVEL) || view->pending_fullscreen() ||
        view->pending_tiled_edges())
    {
        return;
    }

    // Geometry is relative to the desktop in view; a window on a desktop
    // above or below it is held to the top of its own.
    wf::output_t* output = view->get_output();
    auto geometry = view->get_pending_geometry();
    const auto grid = output->wset()->get_workspace_grid_size();
    const auto current = output->wset()->get_current_workspace();
    const double screen_height = output->get_relative_geometry().height;
    const int middle_row = int(std::floor((geometry.y + geometry.height / 2) / screen_height));
    const int row = std::clamp(middle_row, -current.y, grid.height - 1 - current.y);
    const double top = output->workarea->get_workarea().y + row * screen_height;
    if (geometry.y < top)
    {
        geometry.y = top;
        view->set_geometry(geometry);
    }
}

// With its dialogs, which go where it goes and are not in the workspace set.
static void keep_reachable(wayfire_toplevel_view view)
{
    if (!view)
    {
        return;
    }

    for (auto& member : view->enumerate_views())
    {
        keep_one_reachable(member);
    }
}

class screen_t : public wf::per_output_plugin_instance_t
{
    wf::signal::connection_t<wf::view_mapped_signal> on_mapped = [](wf::view_mapped_signal* event) {
        keep_reachable(wf::toplevel_cast(event->view));
    };

    // Wayfire's move and resize send it when they let go of a window.
    wf::signal::connection_t<wf::view_change_workspace_signal> on_let_go = [](wf::view_change_workspace_signal* event) {
        keep_reachable(event->view);
    };

    wf::signal::connection_t<wf::workarea_changed_signal> on_workarea_changed = [=](wf::workarea_changed_signal*) {
        for (auto& view : output->wset()->get_views(wf::WSET_MAPPED_ONLY))
        {
            keep_reachable(view);
        }
    };

  public:
    void init() override
    {
        output->connect(&on_mapped);
        output->connect(&on_let_go);
        output->connect(&on_workarea_changed);
    }

    void fini() override
    {
        on_mapped.disconnect();
        on_let_go.disconnect();
        on_workarea_changed.disconnect();
    }
};
} // namespace reachable_title_bars

DECLARE_WAYFIRE_PLUGIN(wf::per_output_plugin_t<reachable_title_bars::screen_t>);
