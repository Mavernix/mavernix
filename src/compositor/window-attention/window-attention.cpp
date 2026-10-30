// The plugin "window-attention": tells the shell when a window asks for the
// user's attention, so its program's icon in the dock can bounce until the
// user turns to it, as a program's icon does in OS X's Dock.
//
// Only X11 programs can ask (their urgency hint, which Wayfire reports);
// Wayland has no such request yet.
//
// IPC method "window-attention/watch": from then on, the calling program
// gets the event "window-attention", with the window's "id" and "app-id" and
// "wants", each time a window starts or stops asking (or goes away while
// asking). It is told at once of the windows already asking, so a shell
// that restarts misses none.

#include <algorithm>
#include <map>
#include <vector>

#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/view.hpp>

namespace window_attention
{
const std::string WATCH_METHOD = "window-attention/watch";
const std::string EVENT = "window-attention";

class plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repo;
    std::vector<wf::ipc::client_interface_t*> watchers;
    // The windows asking now, by their ID, with their app IDs.
    std::map<uint32_t, std::string> asking;

    static wf::json_t message(uint32_t id, const std::string& app_id, bool wants)
    {
        wf::json_t event;
        event["event"] = EVENT;
        event["id"] = id;
        event["app-id"] = app_id;
        event["wants"] = wants;
        return event;
    }

    void tell_all(uint32_t id, const std::string& app_id, bool wants)
    {
        for (auto client : watchers)
        {
            client->send_json(message(id, app_id, wants));
        }
    }

    wf::ipc::method_callback_full on_watch = [=](const wf::json_t&, wf::ipc::client_interface_t* client)
    {
        if (std::find(watchers.begin(), watchers.end(), client) == watchers.end())
        {
            watchers.push_back(client);
            for (const auto& [id, app_id] : asking)
            {
                client->send_json(message(id, app_id, true));
            }
        }
        return wf::ipc::json_ok();
    };

    wf::signal::connection_t<wf::ipc::client_disconnected_signal> on_client_disconnected =
        [=](wf::ipc::client_disconnected_signal *event)
    {
        watchers.erase(std::remove(watchers.begin(), watchers.end(), event->client), watchers.end());
    };

    // Hints change for other reasons too; only starting or stopping to ask counts.
    wf::signal::connection_t<wf::view_hints_changed_signal> on_hints_changed =
        [=](wf::view_hints_changed_signal *signal)
    {
        if (!signal->view)
        {
            return;
        }
        const uint32_t id = signal->view->get_id();
        const bool was_asking = asking.count(id) > 0;
        if (signal->demands_attention == was_asking)
        {
            return;
        }
        const std::string app_id = signal->view->get_app_id();
        if (signal->demands_attention)
        {
            asking[id] = app_id;
        } else
        {
            asking.erase(id);
        }
        tell_all(id, app_id, signal->demands_attention);
    };

    wf::signal::connection_t<wf::view_unmapped_signal> on_unmapped = [=](wf::view_unmapped_signal *signal)
    {
        const auto found = signal->view ? asking.find(signal->view->get_id()) : asking.end();
        if (found != asking.end())
        {
            const auto [id, app_id] = *found;
            asking.erase(found);
            tell_all(id, app_id, false);
        }
    };

  public:
    void init() override
    {
        repo->register_method(WATCH_METHOD, on_watch);
        repo->connect(&on_client_disconnected);
        wf::get_core().connect(&on_hints_changed);
        wf::get_core().connect(&on_unmapped);
    }

    void fini() override
    {
        on_unmapped.disconnect();
        on_hints_changed.disconnect();
        on_client_disconnected.disconnect();
        repo->unregister_method(WATCH_METHOD);
        watchers.clear();
        asking.clear();
    }
};
} // namespace window_attention

DECLARE_WAYFIRE_PLUGIN(window_attention::plugin_t);
