// The plugin "x11-settings": tells X11 programs about the main screen, the
// one with the menu bar and dock.
//
// "primary_screen": it is made X11's primary screen. Xwayland names none, and
// X11 programs then take the first screen it lists for their own: games
// (SDL, Wine and Proton) open there and offer only that screen's resolutions.
//
// "dpi": X11's Xft.dpi is 96 times its scale, for X11 programs drawn at the
// screen's own pixels (workarounds/force_xwayland_scaling) to size
// themselves. Qt, GTK's text, Chromium and Electron read it as they start.
//
// The plugin talks to Xwayland as an X11 client, as KWin does. It connects
// only once Xwayland says it is ready (core/xwayland_startup_script calls
// x11-settings/xwayland-ready), since connecting earlier would wait on a server
// that cannot start while the compositor waits. Xwayland adds and removes its
// screens after the compositor does, so the plugin sets the primary again
// whenever Xwayland reports a change. Nothing waits for Xwayland's answers.
//
// Which screen is the main one is the shell's to decide ([display]
// main_screen, else the first), so the shell tells it, and again when it
// changes.
//
// IPC: x11-settings/xwayland-ready; x11-settings/main-screen { "name" };
// x11-settings/window-id { "id" } -> { "window" }, an X11 window's own ID.

#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/shared-core-data.hpp>
#include <wayfire/plugins/ipc/ipc-helpers.hpp>
#include <wayfire/plugins/ipc/ipc-method-repository.hpp>
#include <wayfire/util/log.hpp>

namespace x11_settings
{
const std::string READY_METHOD = "x11-settings/xwayland-ready";
const std::string MAIN_SCREEN_METHOD = "x11-settings/main-screen";
const std::string WINDOW_ID_METHOD = "x11-settings/window-id";

class plugin_t : public wf::plugin_interface_t
{
    wf::shared_data::ref_ptr_t<wf::ipc::method_repository_t> repo;
    wf::option_wrapper_t<bool> primary_screen{"x11-settings/primary_screen"};
    wf::option_wrapper_t<bool> dpi{"x11-settings/dpi"};
    // Xft.dpi as last written; 0 if this plugin has not written it.
    int written_dpi = 0;
    // Asked for the resources, to write dpi_wanted when they come.
    bool dpi_asked = false;
    int dpi_wanted = 0;
    xcb_get_property_cookie_t resources_asked;
    // As the shell last said; empty until it does.
    std::string main_screen;

    xcb_connection_t* x11 = nullptr;
    xcb_window_t root = XCB_NONE;
    uint8_t randr_event_base = 0;
    wl_event_source* readable = nullptr;

    // Nothing here waits for Xwayland: a request is sent, and its answer read
    // when it has come (read_events). Waiting could hang the desktop while
    // Xwayland itself waits on the compositor.
    enum class step_t
    {
        none,
        // Whether Xwayland has RandR, and its event numbers.
        randr,
        // Its screens.
        screens,
        // Each screen's name, and which one is primary now.
        names,
    };
    step_t step = step_t::none;
    // Asked for while a lookup was under way: another one follows it.
    bool again = false;
    xcb_query_extension_cookie_t randr_asked;
    xcb_randr_get_screen_resources_current_cookie_t screens_asked;
    std::vector<std::pair<xcb_randr_output_t, xcb_randr_get_output_info_cookie_t>> names_asked;
    size_t names_read = 0;
    xcb_randr_get_output_primary_cookie_t primary_asked;
    xcb_randr_output_t wanted = XCB_NONE;

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
        step = step_t::none;
        again = false;
        names_asked.clear();
        written_dpi = 0;
        dpi_asked = false;
    }

    // Xwayland has said it is ready, so the connection's setup is answered at once.
    void connect()
    {
        disconnect();
        const std::string display = wf::get_core().get_xwayland_display();
        if (display.empty())
        {
            return;
        }
        x11 = xcb_connect(display.c_str(), nullptr);
        if (xcb_connection_has_error(x11))
        {
            LOGE("x11-settings: cannot connect to Xwayland's display ", display);
            disconnect();
            return;
        }
        root = xcb_setup_roots_iterator(xcb_get_setup(x11)).data->root;
        readable = wl_event_loop_add_fd(wf::get_core().ev_loop, xcb_get_file_descriptor(x11), WL_EVENT_READABLE,
            [](int, uint32_t mask, void* self) {
            static_cast<plugin_t*>(self)->read_events(mask);
            return 0;
        }, this);
        const char name[] = "RANDR";
        randr_asked = xcb_query_extension(x11, sizeof(name) - 1, name);
        step = step_t::randr;
        xcb_flush(x11);
    }

    // Whether any of the events read says Xwayland's screens changed.
    bool screens_changed()
    {
        bool changed = false;
        while (xcb_generic_event_t* event = xcb_poll_for_event(x11))
        {
            const uint8_t type = event->response_type & ~0x80;
            changed |= (randr_event_base != 0) && ((type == randr_event_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) ||
                                                      (type == randr_event_base + XCB_RANDR_NOTIFY));
            free(event);
        }
        return changed;
    }

    void read_events(uint32_t mask)
    {
        bool changed = screens_changed();
        // Xwayland ended; a new one says it is ready again.
        if ((mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) || xcb_connection_has_error(x11))
        {
            disconnect();
            return;
        }
        read_answers();
        // An answer may have shown Xwayland unusable, and disconnected.
        if (x11 == nullptr)
        {
            return;
        }
        // Reading answers may have read events behind them, which the socket
        // will not tell of again.
        changed |= screens_changed();
        if (changed)
        {
            apply();
        }
    }

    // Starts finding the main screen among Xwayland's and making it primary.
    void apply()
    {
        if ((x11 == nullptr) || (step == step_t::randr))
        {
            return;
        }
        if (step != step_t::none)
        {
            again = true;
            return;
        }
        screens_asked = xcb_randr_get_screen_resources_current(x11, root);
        step = step_t::screens;
        xcb_flush(x11);
    }

    // Goes on as far as the answers that have come allow.
    void read_answers()
    {
        read_resources();
        if (step == step_t::randr)
        {
            xcb_query_extension_reply_t* randr = nullptr;
            if (!xcb_poll_for_reply(x11, randr_asked.sequence, reinterpret_cast<void**>(&randr), nullptr))
            {
                return;
            }
            const bool present = (randr != nullptr) && randr->present;
            randr_event_base = present ? randr->first_event : 0;
            free(randr);
            if (!present)
            {
                LOGE("x11-settings: Xwayland has no RandR");
                disconnect();
                return;
            }
            xcb_randr_select_input(
                x11, root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE | XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE);
            step = step_t::none;
            apply();
            write_dpi();
        }

        if (step == step_t::screens)
        {
            xcb_randr_get_screen_resources_current_reply_t* screens = nullptr;
            if (!xcb_poll_for_reply(x11, screens_asked.sequence, reinterpret_cast<void**>(&screens), nullptr))
            {
                return;
            }
            if (screens == nullptr)
            {
                finish();
                return;
            }
            names_asked.clear();
            names_read = 0;
            wanted = XCB_NONE;
            const xcb_randr_output_t* outputs = xcb_randr_get_screen_resources_current_outputs(screens);
            const int count = xcb_randr_get_screen_resources_current_outputs_length(screens);
            for (int i = 0; i < count; ++i)
            {
                names_asked.emplace_back(
                    outputs[i], xcb_randr_get_output_info(x11, outputs[i], screens->config_timestamp));
            }
            free(screens);
            primary_asked = xcb_randr_get_output_primary(x11, root);
            step = step_t::names;
            xcb_flush(x11);
        }

        if (step == step_t::names)
        {
            for (; names_read < names_asked.size(); ++names_read)
            {
                const auto& [output, asked] = names_asked[names_read];
                xcb_randr_get_output_info_reply_t* info = nullptr;
                if (!xcb_poll_for_reply(x11, asked.sequence, reinterpret_cast<void**>(&info), nullptr))
                {
                    return;
                }
                if (info != nullptr)
                {
                    const auto* bytes = reinterpret_cast<const char*>(xcb_randr_get_output_info_name(info));
                    const std::string name(bytes, xcb_randr_get_output_info_name_length(info));
                    if (primary_screen && (name == main_screen))
                    {
                        wanted = output;
                    }
                    free(info);
                }
            }
            xcb_randr_get_output_primary_reply_t* primary = nullptr;
            if (!xcb_poll_for_reply(x11, primary_asked.sequence, reinterpret_cast<void**>(&primary), nullptr))
            {
                return;
            }
            if ((primary == nullptr) || (primary->output != wanted))
            {
                xcb_randr_set_output_primary(x11, root, wanted);
                xcb_flush(x11);
            }
            free(primary);
            finish();
        }
    }

    // X11 programs read it from the root window's RESOURCE_MANAGER, as xrdb
    // leaves it; nothing else is kept there under Xwayland.
    // Xft.dpi is one line among the resources; others (xrdb's) stay. The
    // resources are read first, and the line written when they have come.
    void write_dpi()
    {
        if ((x11 == nullptr) || (step == step_t::randr))
        {
            return;
        }
        wf::output_t* screen = main_screen.empty() ? nullptr : wf::get_core().output_layout->find_output(main_screen);
        const int wanted = (dpi && screen) ? int(std::lround(96 * screen->handle->scale)) : 0;
        if ((wanted == written_dpi) && !dpi_asked)
        {
            return;
        }
        dpi_wanted = wanted;
        if (!dpi_asked)
        {
            resources_asked = xcb_get_property(x11, 0, root, XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 0, 1 << 20);
            dpi_asked = true;
            xcb_flush(x11);
        }
    }

    void read_resources()
    {
        xcb_get_property_reply_t* reply = nullptr;
        if (!dpi_asked || !xcb_poll_for_reply(x11, resources_asked.sequence, reinterpret_cast<void**>(&reply), nullptr))
        {
            return;
        }
        dpi_asked = false;
        std::string resources;
        if (reply != nullptr)
        {
            resources.assign(
                static_cast<const char*>(xcb_get_property_value(reply)), size_t(xcb_get_property_value_length(reply)));
            free(reply);
        }
        if (dpi_wanted == written_dpi)
        {
            return;
        }

        // Without this plugin's line: replaced, or taken away if the plugin wrote it.
        std::string kept;
        std::istringstream lines(resources);
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.rfind("Xft.dpi:", 0) != 0)
            {
                kept += line + "\n";
            }
        }
        if (dpi_wanted > 0)
        {
            kept += "Xft.dpi:\t" + std::to_string(dpi_wanted) + "\n";
        }
        if (kept.empty())
        {
            xcb_delete_property(x11, root, XCB_ATOM_RESOURCE_MANAGER);
        }
        else
        {
            xcb_change_property(x11, XCB_PROP_MODE_REPLACE, root, XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 8,
                uint32_t(kept.size()), kept.data());
        }
        xcb_flush(x11);
        written_dpi = dpi_wanted;
    }

    // A screen's scale may have changed.
    wf::signal::connection_t<wf::output_layout_configuration_changed_signal> on_layout_changed = [this](auto) {
        write_dpi();
    };

    void finish()
    {
        step = step_t::none;
        names_asked.clear();
        if (again)
        {
            again = false;
            apply();
        }
    }

    wf::ipc::method_callback on_ready = [this](const wf::json_t&) {
        connect();
        return wf::ipc::json_ok();
    };

    // An X11 window's own ID, which X11 programs name their menu by
    // (the shell's menu registrar); 0 for a Wayland window.
    wf::ipc::method_callback on_window_id = [](const wf::json_t& data) {
        wayfire_view view = wf::ipc::json_find_view_or_throw(data);
        wlr_surface* surface = view->get_wlr_surface();
        wlr_xwayland_surface* x11_window = surface ? wlr_xwayland_surface_try_from_wlr_surface(surface) : nullptr;
        wf::json_t answer = wf::ipc::json_ok();
        answer["window"] = x11_window ? int64_t(x11_window->window_id) : int64_t(0);
        return answer;
    };

    wf::ipc::method_callback on_main_screen = [this](const wf::json_t& data) {
        if (!data.has_member("name") || !data["name"].is_string())
        {
            return wf::ipc::json_error("x11-settings/main-screen needs a \"name\"");
        }
        main_screen = data["name"].as_string();
        apply();
        write_dpi();
        return wf::ipc::json_ok();
    };

  public:
    void init() override
    {
        repo->register_method(READY_METHOD, on_ready);
        repo->register_method(MAIN_SCREEN_METHOD, on_main_screen);
        repo->register_method(WINDOW_ID_METHOD, on_window_id);
        primary_screen.set_callback([this] { apply(); });
        dpi.set_callback([this] { write_dpi(); });
        wf::get_core().output_layout->connect(&on_layout_changed);
    }

    void fini() override
    {
        repo->unregister_method(READY_METHOD);
        repo->unregister_method(MAIN_SCREEN_METHOD);
        repo->unregister_method(WINDOW_ID_METHOD);
        disconnect();
    }
};
} // namespace x11_settings

DECLARE_WAYFIRE_PLUGIN(x11_settings::plugin_t);
