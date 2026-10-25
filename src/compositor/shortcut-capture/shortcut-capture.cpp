// The plugin "shortcut-capture": switches the compositor's key bindings off
// while the settings window records a shortcut, so the keys reach it
// (as Wayfire's "shortcuts-inhibit" does for the protocol).
//
// IPC: shortcut-capture/start (error if another program is recording) and
// shortcut-capture/stop. The client keeps its connection open meanwhile;
// a closed connection, the timeout or unloading switches the keys back on.

#include <wayfire/bindings-repository.hpp>
#include <wayfire/core.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/util.hpp>

#include "capture-holder.hpp"

namespace shortcut_capture
{
const std::string PLUGIN_NAME = "shortcut-capture";
const std::string START_METHOD = PLUGIN_NAME + "/start";
const std::string STOP_METHOD = PLUGIN_NAME + "/stop";

class plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repo;
    wf::option_wrapper_t<int> timeout_ms{PLUGIN_NAME + "/timeout_ms"};
    wf::wl_timer<false> time_is_up;

    // The bindings' switch is counted: each off must be matched by one on.
    holder_t holder{[](bool enabled) { wf::get_core().bindings->set_enabled(enabled); }};

    wf::ipc::method_callback_full on_start = [=](const wf::json_t&, wf::ipc::client_interface_t* client) {
        if (!holder.start(client))
        {
            return wf::ipc::json_error("another program is recording a shortcut");
        }

        time_is_up.set_timeout(timeout_ms, [=]() {
            LOGW("shortcut-capture: time ran out, switching the keys on again");
            holder.release();
        });
        return wf::ipc::json_ok();
    };

    wf::ipc::method_callback_full on_stop = [=](const wf::json_t&, wf::ipc::client_interface_t* client) {
        if (!holder.stop(client))
        {
            return wf::ipc::json_error("this program is not recording a shortcut");
        }

        time_is_up.disconnect();
        return wf::ipc::json_ok();
    };

    wf::signal::connection_t<wf::ipc::client_disconnected_signal> on_client_disconnected =
        [=](wf::ipc::client_disconnected_signal* ev) {
        holder.connection_closed(ev->client);
        if (!holder.is_recording())
        {
            time_is_up.disconnect();
        }
    };

  public:
    void init() override
    {
        repo->register_method(START_METHOD, on_start);
        repo->register_method(STOP_METHOD, on_stop);
        repo->connect(&on_client_disconnected);
    }

    void fini() override
    {
        on_client_disconnected.disconnect();
        repo->unregister_method(START_METHOD);
        repo->unregister_method(STOP_METHOD);
        time_is_up.disconnect();
        holder.release();
    }
};
} // namespace shortcut_capture

DECLARE_WAYFIRE_PLUGIN(shortcut_capture::plugin_t);
