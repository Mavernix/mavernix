#include <wayfire/per-output-plugin.hpp>
#include <wayfire/view.hpp>
#include <wayfire/workarea.hpp>
#include <wayfire/matcher.hpp>
#include <wayfire/workspace-set.hpp>
#include <wayfire/output.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/txn/transaction-manager.hpp>

// Not through wlroots-full.hpp, which leaves it out unless the generated
// xdg-shell protocol header is present; wlroots 0.20 no longer needs it.
extern "C"
{
#include <wlr/types/wlr_xdg_decoration_v1.h>
}

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "deco-subsurface.hpp"
#include "wayfire/core.hpp"
#include "wayfire/plugin.hpp"
#include "wayfire/signal-provider.hpp"
#include "wayfire/toplevel-view.hpp"
#include "wayfire/toplevel.hpp"

// An app can ask for a joined tool bar itself, in its desktop entry.
static bool app_asks_to_join(const std::string& app_id)
{
    const char *home = std::getenv("HOME");
    const char *data_dirs = std::getenv("XDG_DATA_DIRS");
    std::stringstream dirs(std::string(home ? home : "") + "/.local/share:" +
        (data_dirs ? data_dirs : "/usr/local/share:/usr/share"));

    std::string dir;
    while (std::getline(dirs, dir, ':'))
    {
        std::ifstream entry(dir + "/applications/" + app_id + ".desktop");
        std::string line;
        while (std::getline(entry, line))
        {
            if (line == "X-Mavernix-JoinedToolBar=true")
            {
                return true;
            }
        }
    }

    return false;
}

class wayfire_decoration : public wf::plugin_interface_t
{
    wf::view_matcher_t ignore_views{"themed-decoration/ignore_views"};
    wf::view_matcher_t forced_views{"themed-decoration/forced_views"};
    wf::view_matcher_t joined_views{"themed-decoration/joined_views"};
    wf::option_wrapper_t<std::string> theme_directory{"themed-decoration/theme"};
    wf::option_wrapper_t<std::string> preferred_mode{"core/preferred_decoration_mode"};
    wf::option_wrapper_t<bool> force_preferred_mode{"workarounds/force_preferred_decoration_mode"};
    std::string theme_in_use;

    // Frames are remade after the whole reload, not on the theme option
    // alone, or they would take the old button side and order.
    wf::signal::connection_t<wf::reload_config_signal> on_config_reloaded =
        [=] (wf::reload_config_signal*)
    {
        if (std::string(theme_directory) != theme_in_use)
        {
            theme_in_use = theme_directory;
            redecorate_all_views();
        }
    };

    wf::signal::connection_t<wf::txn::new_transaction_signal> on_new_tx =
        [=] (wf::txn::new_transaction_signal *ev)
    {
        // For each transaction, we need to consider what happens with participating views
        for (const auto& obj : ev->tx->get_objects())
        {
            if (auto toplevel = std::dynamic_pointer_cast<wf::toplevel_t>(obj))
            {
                // First check whether the toplevel already has decoration
                // In that case, we should just set the correct margins
                if (auto deco = toplevel->get_data<wf::simple_decorator_t>())
                {
                    toplevel->pending().margins = deco->get_margins(toplevel->pending());
                    continue;
                }

                // Second case: the view is already mapped, or the transaction does not map it.
                // The view is not being decorated, so nothing to do here.
                if (toplevel->current().mapped || !toplevel->pending().mapped)
                {
                    continue;
                }

                // Third case: the transaction will map the toplevel.
                auto view = wf::find_view_for_toplevel(toplevel);
                wf::dassert(view != nullptr, "Mapping a toplevel means there must be a corresponding view!");
                if (should_decorate_view(view))
                {
                    adjust_new_decorations(view);
                }
            }
        }
    };

    wf::signal::connection_t<wf::view_decoration_state_updated_signal> on_decoration_state_changed =
        [=] (wf::view_decoration_state_updated_signal *ev)
    {
        update_view_decoration(ev->view);
    };

    // allows criteria containing maximized or floating check
    wf::signal::connection_t<wf::view_tiled_signal> on_view_tiled =
        [=] (wf::view_tiled_signal *ev)
    {
        update_view_decoration(ev->view);
    };

  public:
    void init() override
    {
        wf::get_core().connect(&on_decoration_state_changed);
        wf::get_core().tx_manager->connect(&on_new_tx);
        wf::get_core().connect(&on_view_tiled);
        theme_in_use = theme_directory;
        wf::get_core().connect(&on_config_reloaded);

        for (auto& view : wf::get_core().get_all_views())
        {
            update_view_decoration(view);
        }
    }

    /**
     * A frame reads its theme only when it is made, so a theme change needs
     * every decorated window to get a new frame.
     */
    void redecorate_all_views()
    {
        for (auto& view : wf::get_core().get_all_views())
        {
            auto toplevel = wf::toplevel_cast(view);
            if (!toplevel || !is_toplevel_decorated(toplevel->toplevel()))
            {
                continue;
            }

            remove_decoration(toplevel);
            adjust_new_decorations(toplevel);
            wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
        }
    }

    void fini() override
    {
        for (auto view : wf::get_core().get_all_views())
        {
            if (auto toplevel = wf::toplevel_cast(view))
            {
                remove_decoration(toplevel);
                wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
            }
        }
    }

    bool ignore_decoration_of_view(wayfire_view view)
    {
        return ignore_views.matches(view);
    }

    bool force_decoration_of_view(wayfire_view view)
    {
        return forced_views.matches(view);
    }

    bool should_decorate_view(wayfire_toplevel_view view)
    {
        return !ignore_decoration_of_view(view) &&
               (force_decoration_of_view(view) || wants_server_side(view));
    }

    /**
     * Wayfire counts a window as server-side until it has agreed a decoration
     * mode, and learns the agreed mode only after the commit that agreed it,
     * which is also the commit that shows the window. A window told to draw
     * its own frame would get ours until then, and its first configures would
     * be our frame's size short; Discord shown again from the tray never
     * answers that, so it never appears. So the window's xdg-decoration state
     * decides: the agreed mode, or else the mode Wayfire is about to send,
     * chosen as Wayfire's xdg-decoration handler chooses it.
     */
    bool wants_server_side(wayfire_toplevel_view view)
    {
        wlr_xdg_toplevel_decoration_v1 *decoration;
        wl_list_for_each(decoration, &wf::get_core().protocols.xdg_decorator->decorations, link)
        {
            if (decoration->toplevel->base->data != static_cast<wf::view_interface_t*>(view.get()))
            {
                continue;
            }

            auto mode = decoration->current.mode;
            if (mode != WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE)
            {
                return mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
            }

            mode = decoration->requested_mode;
            if ((mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_NONE) || force_preferred_mode)
            {
                mode = (std::string(preferred_mode) == "server") ?
                    WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE :
                    WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
            }

            return mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        }

        return view->should_be_decorated();
    }

    void adjust_new_decorations(wayfire_toplevel_view view)
    {
        auto toplevel = view->toplevel();

        toplevel->store_data(std::make_unique<wf::simple_decorator_t>(view,
            joined_views.matches(view) || app_asks_to_join(view->get_app_id())));
        auto deco     = toplevel->get_data<wf::simple_decorator_t>();
        auto& pending = toplevel->pending();
        pending.margins = deco->get_margins(pending);

        if (!pending.fullscreen && !pending.tiled_edges)
        {
            pending.geometry = wf::expand_geometry_by_margins(pending.geometry, pending.margins);
            if (view->get_output())
            {
                pending.geometry = wf::clamp(pending.geometry, view->get_output()->workarea->get_workarea());
            }
        }
    }

    void remove_decoration(wayfire_toplevel_view view)
    {
        view->toplevel()->erase_data<wf::simple_decorator_t>();
        auto& pending = view->toplevel()->pending();
        if (!pending.fullscreen && !pending.tiled_edges)
        {
            pending.geometry = wf::shrink_geometry_by_margins(pending.geometry, pending.margins);
        }

        pending.margins = {0, 0, 0, 0};
    }

    bool is_toplevel_decorated(const std::shared_ptr<wf::toplevel_t>& toplevel)
    {
        return toplevel->has_data<wf::simple_decorator_t>();
    }

    void update_view_decoration(wayfire_view view)
    {
        if (auto toplevel = wf::toplevel_cast(view))
        {
            const bool wants_decoration = should_decorate_view(toplevel);
            if (wants_decoration != is_toplevel_decorated(toplevel->toplevel()))
            {
                if (wants_decoration)
                {
                    adjust_new_decorations(toplevel);
                } else
                {
                    remove_decoration(toplevel);
                }

                wf::get_core().tx_manager->schedule_object(toplevel->toplevel());
            }
        }
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_decoration);
