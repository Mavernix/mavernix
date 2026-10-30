// The plugin "window-attention": tells the shell when a window asks for the
// user's attention, so its program's icon in the dock can bounce until the
// user turns to it, as a program's icon does in OS X's Dock.
//
// Only X11 programs can ask (their urgency hint, which Wayfire reports);
// Wayland has no such request yet.
//
// IPC method "window-attention/watch": from then on, the calling program
// gets the event "window-attention" with the window's "id" and "app-id" and
// "wants" (true while it asks), whenever a window's hints change, and with
// "wants" false when a window goes away.

#include <algorithm>
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

    wf::ipc::method_callback_full on_watch = [=](const wf::json_t&, wf::ipc::client_interface_t* client)
    {
        if (std::find(watchers.begin(), watchers.end(), client) == watchers.end())
        {
            watchers.push_back(client);
        }
        return wf::ipc::json_ok();
    };

    wf::signal::connection_t<wf::ipc::client_disconnected_signal> on_client_disconnected =
        [=](wf::ipc::client_disconnected_signal *event)
    {
        watchers.erase(std::remove(watchers.begin(), watchers.end(), event->client), watchers.end());
    };

    void tell(wayfire_view view, bool wants)
    {
        if (!view || watchers.empty())
        {
            return;
        }

        wf::json_t message;
        message["event"] = EVENT;
        message["id"] = view->get_id();
        message["app-id"] = view->get_app_id();
        message["wants"] = wants;
        for (auto client : watchers)
        {
            client->send_json(message);
        }
    }

    wf::signal::connection_t<wf::view_hints_changed_signal> on_hints_changed =
        [=](wf::view_hints_changed_signal *signal) { tell(signal->view, signal->demands_attention); };

    wf::signal::connection_t<wf::view_unmapped_signal> on_unmapped =
        [=](wf::view_unmapped_signal *signal) { tell(signal->view, false); };

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
    }
};
} // namespace window_attention

DECLARE_WAYFIRE_PLUGIN(window_attention::plugin_t);
