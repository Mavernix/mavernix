// The plugin "game-vrr": variable refresh rate on a screen only while a
// full-screen window shows there, which is how games run. Always on, the
// desktop can flicker on some screens as its frame rate falls when nothing
// moves; Wayfire's own [output:NAME] vrr is that, and serves "always".
//
// Option "screens": the names of the screens to do this for, separated by
// spaces. A screen that cannot change its refresh rate is left alone.
//
// Wayfire sets every screen as its config says when the config is read
// again, so the plugin sets its screens again after that.

#include <sstream>
#include <string>

#include <wayfire/config-backend.hpp>
#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/per-output-plugin.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/util.hpp>
#include <wayfire/workspace-set.hpp>

namespace game_vrr
{
static bool listed(const std::string& names, const std::string& name)
{
    std::istringstream words(names);
    std::string word;
    while (words >> word)
    {
        if (word == name)
        {
            return true;
        }
    }
    return false;
}

class screen_t : public wf::per_output_plugin_instance_t
{
    wf::option_wrapper_t<std::string> screens{"game-vrr/screens"};
    // Whether this plugin turned it on, to turn it off again.
    bool turned_on = false;
    // The screen is being unplugged: its settings are no longer to be changed.
    bool going = false;
    wf::wl_idle_call idle_update;

    bool full_screen_shown()
    {
        for (auto& view :
            output->wset()->get_views(wf::WSET_MAPPED_ONLY | wf::WSET_EXCLUDE_MINIMIZED | wf::WSET_CURRENT_WORKSPACE))
        {
            if (view->pending_fullscreen())
            {
                return true;
            }
        }
        return false;
    }

    void set(bool on)
    {
        auto layout = wf::get_core().output_layout.get();
        auto configuration = layout->get_current_configuration();
        auto entry = configuration.find(output->handle);
        if ((entry == configuration.end()) || (entry->second.vrr == on))
        {
            turned_on = on && (entry != configuration.end());
            return;
        }
        entry->second.vrr = on;
        layout->apply_configuration(configuration);
        turned_on = on;
    }

    // [output:NAME] vrr, which is Wayfire's to keep.
    bool always_on()
    {
        auto option = wf::get_core().config_backend->get_output_section(output->handle)->get_option_or("vrr");
        return option && (option->get_value_str() == "true");
    }

    void update()
    {
        if (going)
        {
            return;
        }
        const bool wanted = listed(screens, output->to_string());
        if (!wanted)
        {
            // Off only if this plugin turned it on: "always" is the config's.
            if (turned_on)
            {
                set(false);
            }
            return;
        }
        if (!output->handle->adaptive_sync_supported)
        {
            return;
        }
        set(full_screen_shown());
    }

    // After whatever caused it has settled: a window becomes full screen
    // before it is drawn so.
    void update_soon()
    {
        idle_update.run_once([this] { update(); });
    }

    wf::signal::connection_t<wf::output_pre_remove_signal> on_going = [this](auto) {
        going = true;
        idle_update.disconnect();
    };

    wf::signal::connection_t<wf::view_fullscreen_signal> on_fullscreen = [this](auto) { update_soon(); };
    wf::signal::connection_t<wf::view_minimized_signal> on_minimized = [this](auto) { update_soon(); };
    wf::signal::connection_t<wf::workspace_changed_signal> on_workspace = [this](auto) { update_soon(); };
    wf::signal::connection_t<wf::view_mapped_signal> on_mapped = [this](auto) { update_soon(); };
    wf::signal::connection_t<wf::view_unmapped_signal> on_unmapped = [this](auto) { update_soon(); };
    // A window moved to or from another screen.
    wf::signal::connection_t<wf::view_set_output_signal> on_set_output = [this](auto) { update_soon(); };
    // Wayfire has set the screen as the config says.
    wf::signal::connection_t<wf::reload_config_signal> on_reload = [this](auto) {
        turned_on = false;
        update_soon();
    };

  public:
    void init() override
    {
        output->connect(&on_going);
        output->connect(&on_fullscreen);
        output->connect(&on_minimized);
        output->connect(&on_workspace);
        output->connect(&on_mapped);
        wf::get_core().connect(&on_unmapped);
        wf::get_core().connect(&on_set_output);
        wf::get_core().connect(&on_reload);
        screens.set_callback([this] { update_soon(); });
        update_soon();
    }

    void fini() override
    {
        idle_update.disconnect();
        // Unloaded perhaps because the screen is now to have it always.
        if (turned_on && !going && !always_on())
        {
            set(false);
        }
    }
};
} // namespace game_vrr

DECLARE_WAYFIRE_PLUGIN(wf::per_output_plugin_t<game_vrr::screen_t>);
