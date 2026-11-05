// The plugin "x11-primary": makes the main screen, the one with the menu bar
// and dock, X11's primary screen. Xwayland names none, and X11 programs then
// take the first screen it lists for their own: games (SDL, Wine and Proton)
// open there and offer only that screen's resolutions.
//
// The plugin talks to Xwayland as an X11 client, as KWin does. It connects
// only once Xwayland says it is ready (core/xwayland_startup_script calls
// x11-primary/xwayland-ready), since connecting earlier would wait on a server
// that cannot start while the compositor waits. Xwayland adds and removes its
// screens after the compositor does, so the plugin sets the primary again
// whenever Xwayland reports a change.
//
// Which screen is the main one is the shell's to decide ([display]
// main_screen, else the first), so the shell tells it, and again when it
// changes. Option "enabled": off leaves X11 without a primary screen.
//
// IPC: x11-primary/xwayland-ready; x11-primary/main-screen { "name" }.

#include <string>

#include <xcb/randr.h>
#include <xcb/xcb.h>

#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/util/log.hpp>

namespace x11_primary
{
const std::string READY_METHOD = "x11-primary/xwayland-ready";
const std::string MAIN_SCREEN_METHOD = "x11-primary/main-screen";

class plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repo;
    wf::option_wrapper_t<bool> enabled{"x11-primary/enabled"};
    // As the shell last said; empty until it does.
    std::string main_screen;

    xcb_connection_t* x11 = nullptr;
    xcb_window_t root = XCB_NONE;
    uint8_t randr_event_base = 0;
    wl_event_source* readable = nullptr;

    void disconnect()
    {
        if (readable != nullptr)
        {
            wl_event_source_remove(readable);
            readable = nullptr;
        }
        if (x11 != nullptr)
        {
            xcb_disconnect(x11);
            x11 = nullptr;
        }
    }

    void connect()
    {
        disconnect();
        const std::string display = wf::get_core().get_xwayland_display();
        if (display.empty())
        {
            return;
        }

        x11 = xcb_connect(display.c_str(), nullptr);
        const xcb_query_extension_reply_t* randr =
            xcb_connection_has_error(x11) ? nullptr : xcb_get_extension_data(x11, &xcb_randr_id);
        if ((randr == nullptr) || !randr->present)
        {
            LOGE("x11-primary: cannot use RandR on Xwayland's display ", display);
            disconnect();
            return;
        }
        // RandR 1.3 has the primary screen; the server answers with what it has.
        xcb_randr_query_version_reply_t* version =
            xcb_randr_query_version_reply(x11, xcb_randr_query_version(x11, 1, 3), nullptr);
        const bool new_enough = (version != nullptr) && ((version->major_version > 1) || (version->minor_version >= 3));
        free(version);
        if (!new_enough)
        {
            LOGE("x11-primary: Xwayland's RandR is older than 1.3");
            disconnect();
            return;
        }

        randr_event_base = randr->first_event;
        root = xcb_setup_roots_iterator(xcb_get_setup(x11)).data->root;
        xcb_randr_select_input(x11, root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE | XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE);
        readable = wl_event_loop_add_fd(wf::get_core().ev_loop, xcb_get_file_descriptor(x11), WL_EVENT_READABLE,
            [](int, uint32_t mask, void* self) {
            static_cast<plugin_t*>(self)->read_events(mask);
            return 0;
        }, this);
        apply();
    }

    void read_events(uint32_t mask)
    {
        bool changed = false;
        while (xcb_generic_event_t* event = xcb_poll_for_event(x11))
        {
            const uint8_t type = event->response_type & ~0x80;
            changed |= (type == randr_event_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) ||
                       (type == randr_event_base + XCB_RANDR_NOTIFY);
            free(event);
        }
        // Xwayland ended; a new one says it is ready again.
        if ((mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) || xcb_connection_has_error(x11))
        {
            disconnect();
            return;
        }
        if (changed)
        {
            apply();
        }
    }

    // The X11 screen named `name`, else XCB_NONE.
    xcb_randr_output_t find_output(const std::string& name)
    {
        xcb_randr_get_screen_resources_current_reply_t* resources = xcb_randr_get_screen_resources_current_reply(
            x11, xcb_randr_get_screen_resources_current(x11, root), nullptr);
        if (resources == nullptr)
        {
            return XCB_NONE;
        }

        xcb_randr_output_t found = XCB_NONE;
        const xcb_randr_output_t* outputs = xcb_randr_get_screen_resources_current_outputs(resources);
        const int count = xcb_randr_get_screen_resources_current_outputs_length(resources);
        for (int i = 0; (i < count) && (found == XCB_NONE); ++i)
        {
            xcb_randr_get_output_info_reply_t* info = xcb_randr_get_output_info_reply(
                x11, xcb_randr_get_output_info(x11, outputs[i], resources->config_timestamp), nullptr);
            if (info != nullptr)
            {
                const auto* bytes = reinterpret_cast<const char*>(xcb_randr_get_output_info_name(info));
                const std::string output_name(bytes, xcb_randr_get_output_info_name_length(info));
                if (output_name == name)
                {
                    found = outputs[i];
                }
                free(info);
            }
        }
        free(resources);
        return found;
    }

    void apply()
    {
        if (x11 == nullptr)
        {
            return;
        }

        xcb_randr_output_t wanted = XCB_NONE;
        if (enabled && !main_screen.empty())
        {
            wanted = find_output(main_screen);
        }

        xcb_randr_get_output_primary_reply_t* primary =
            xcb_randr_get_output_primary_reply(x11, xcb_randr_get_output_primary(x11, root), nullptr);
        const bool same = (primary != nullptr) && (primary->output == wanted);
        free(primary);
        if (!same)
        {
            xcb_randr_set_output_primary(x11, root, wanted);
            xcb_flush(x11);
        }
    }

    wf::ipc::method_callback on_ready = [this](const wf::json_t&) {
        connect();
        return wf::ipc::json_ok();
    };

    wf::ipc::method_callback on_main_screen = [this](const wf::json_t& data) {
        if (!data.has_member("name") || !data["name"].is_string())
        {
            return wf::ipc::json_error("x11-primary/main-screen needs a \"name\"");
        }
        main_screen = data["name"].as_string();
        apply();
        return wf::ipc::json_ok();
    };

  public:
    void init() override
    {
        repo->register_method(READY_METHOD, on_ready);
        repo->register_method(MAIN_SCREEN_METHOD, on_main_screen);
        enabled.set_callback([this] { apply(); });
    }

    void fini() override
    {
        repo->unregister_method(READY_METHOD);
        repo->unregister_method(MAIN_SCREEN_METHOD);
        disconnect();
    }
};
} // namespace x11_primary

DECLARE_WAYFIRE_PLUGIN(x11_primary::plugin_t);
