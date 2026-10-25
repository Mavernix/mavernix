// The plugin "snap-keys": arrow keys that put the window in front on a half,
// a quarter or the whole screen (Left, then Up, gives the upper left
// quarter). The decision is in snap-steps.cpp; the move itself is a tile
// request, which Wayfire's "grid" plugin places and animates.

#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/ipc/ipc-activator.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/window-manager.hpp>
#include <wlr/util/edges.h>

#include "snap-steps.hpp"

// snap-steps keeps its own copy of the edges so it needs nothing of Wayfire.
static_assert(snap_keys::EDGE_TOP == WLR_EDGE_TOP, "snap_keys::EDGE_TOP differs from wlroots'");
static_assert(snap_keys::EDGE_BOTTOM == WLR_EDGE_BOTTOM, "snap_keys::EDGE_BOTTOM differs from wlroots'");
static_assert(snap_keys::EDGE_LEFT == WLR_EDGE_LEFT, "snap_keys::EDGE_LEFT differs from wlroots'");
static_assert(snap_keys::EDGE_RIGHT == WLR_EDGE_RIGHT, "snap_keys::EDGE_RIGHT differs from wlroots'");

namespace snap_keys
{
class plugin_t : public wf::plugin_interface_t
{
    wf::ipc_activator_t keys[4] = {
        wf::ipc_activator_t{"snap-keys/left"},
        wf::ipc_activator_t{"snap-keys/right"},
        wf::ipc_activator_t{"snap-keys/up"},
        wf::ipc_activator_t{"snap-keys/down"},
    };
    const snap_direction directions[4] = {
        snap_direction::LEFT,
        snap_direction::RIGHT,
        snap_direction::UP,
        snap_direction::DOWN,
    };

    static bool can_snap(wayfire_toplevel_view view)
    {
        const uint32_t needed = wf::VIEW_ALLOW_MOVE | wf::VIEW_ALLOW_RESIZE;
        return view->is_mapped() && view->get_output() && !view->pending_fullscreen() &&
               (view->get_allowed_actions() & needed) == needed;
    }

    bool snap(wayfire_view any_view, snap_direction direction)
    {
        auto view = toplevel_cast(any_view);
        if (!view || !can_snap(view))
        {
            // Leave the key to whatever else wants it, as grid does.
            return false;
        }

        const snap_step step = next_snap_step(view->pending_tiled_edges(), direction);
        switch (step.kind)
        {
        case snap_kind::NOTHING:
            break;

        case snap_kind::TILE:
            wf::get_core().default_wm->tile_request(view, step.edges);
            break;

        case snap_kind::RESTORE:
            wf::get_core().default_wm->tile_request(view, 0);
            break;

        case snap_kind::MINIMIZE:
            wf::get_core().default_wm->minimize_request(view, true);
            break;
        }

        return true;
    }

  public:
    void init() override
    {
        for (int index = 0; index < 4; index++)
        {
            const snap_direction direction = directions[index];
            keys[index].set_handler([=](wf::output_t*, wayfire_view view) { return snap(view, direction); });
        }
    }
};
} // namespace snap_keys

DECLARE_WAYFIRE_PLUGIN(snap_keys::plugin_t);
