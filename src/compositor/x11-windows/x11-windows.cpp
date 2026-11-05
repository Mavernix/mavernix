// The plugin "x11-windows": X11 windows where and as their programs ask.
//
// "games": X11 games get the screen they ask for. Wayfire puts a new X11
// window on the focused screen, so a game that goes full screen by itself
// (Wine and Proton do) is moved to the screen holding the middle of what it
// asked for, its primary one. And a window without a frame that asks to
// cover a whole screen (a game's "borderless full screen") is made full
// screen there; Wayfire would fit it between the menu bar and the dock.
// KWin and Mutter treat such windows the same way. A maximized window without
// a frame (Steam draws its own) stays a window. The size is compared in X11's
// units: device pixels with workarounds/force_xwayland_scaling, when
// positions are each screen's own and the window's own screen is taken.
//
// "moves_need_button": a window may ask to be moved or resized only while a
// mouse button or a finger is down, as when its own title bar is dragged.
// Wayfire's move plugin would start a move at any time, from wherever the
// last press was, even on another screen, and hold the window until the next
// click: Steam asks so at times as it shows itself again from its tray menu.
// Such a request is refused by holding the move plugin's kind of activation
// on the window's screen while Wayfire handles it; this plugin hears X11
// windows' requests before Wayfire does.
//
// Wayfire changes the size a window asks for before anyone else sees it, so
// the plugin notes each X11 window's own wish: its size when made, and every
// resize it asks for afterwards, when it looks again: a game may switch to
// borderless in its options, and back to a window, which the plugin then
// undoes if the full screen was its doing.

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
// Only pointed to there; its header needs files this build does not have.
struct wlr_xdg_surface;
#include <wayfire/unstable/wlr-view-events.hpp>
#include <wayfire/util.hpp>
#include <wayfire/view.hpp>
#include <wayfire/window-manager.hpp>

namespace x11_windows
{
#if WF_HAS_XWAYLAND
// What one X11 window last asked for, in X11's coordinates, which are the
// desktop's at scale 1.
static wf::geometry_t box(int x, int y, int width, int height)
{
    return {double(x), double(y), double(width), double(height)};
}

struct wish_t
{
    wf::geometry_t asked;
    // This plugin made the window full screen.
    bool made_full_screen = false;
    wf::wl_listener_wrapper on_configure;
    wf::wl_listener_wrapper on_move;
    wf::wl_listener_wrapper on_resize;
    wf::wl_listener_wrapper on_destroy;
};

class plugin_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<bool> games{"x11-windows/games"};
    wf::option_wrapper_t<bool> moves_need_button{"x11-windows/moves_need_button"};

    // Mouse buttons and fingers down now.
    int presses = 0;
    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_button_event>> on_button =
        [this](wf::input_event_signal<wlr_pointer_button_event>* event) {
        presses = std::max(0, presses + (event->event->state == WL_POINTER_BUTTON_STATE_PRESSED ? 1 : -1));
    };
    wf::signal::connection_t<wf::input_event_signal<wlr_touch_down_event>> on_finger_down = [this](auto) { presses++; };
    wf::signal::connection_t<wf::input_event_signal<wlr_touch_up_event>> on_finger_up = [this](auto) {
        presses = std::max(0, presses - 1);
    };

    // Held while Wayfire handles a move or resize asked for with nothing down.
    wf::plugin_activation_data_t refusing{
        .name = "x11-windows",
        .capabilities = wf::CAPABILITY_GRAB_INPUT | wf::CAPABILITY_MANAGE_DESKTOP,
    };
    wf::output_t* refusing_on = nullptr;
    wf::wl_idle_call stop_refusing;

    void refuse_unless_pressed(wlr_xwayland_surface* surface)
    {
        if ((presses > 0) || !moves_need_button || (refusing_on != nullptr) || (surface->surface == nullptr))
        {
            return;
        }
        wayfire_view view = wf::wl_surface_to_wayfire_view(surface->surface->resource);
        wf::output_t* output = view ? view->get_output() : nullptr;
        if (output && output->activate_plugin(&refusing))
        {
            refusing_on = output;
            stop_refusing.run_once([this] {
                refusing_on->deactivate_plugin(&refusing);
                refusing_on = nullptr;
            });
        }
    }
    std::map<wlr_xwayland_surface*, std::unique_ptr<wish_t>> wishes;
    // A wish is freed only after its window's destroy listener has run.
    std::vector<std::unique_ptr<wish_t>> gone;
    wf::wl_idle_call free_gone;

    wf::signal::connection_t<wf::new_xwayland_surface_signal> on_new_surface =
        [this](wf::new_xwayland_surface_signal* event) {
        wlr_xwayland_surface* surface = event->surface;
        auto wish = std::make_unique<wish_t>();
        wish->asked = box(surface->x, surface->y, surface->width, surface->height);
        wish->on_configure.set_callback([this, wish = wish.get(), surface](void* data) {
            const auto* asked = static_cast<wlr_xwayland_surface_configure_event*>(data);
            wish->asked = box(asked->x, asked->y, asked->width, asked->height);
            consider(surface->surface ? wf::toplevel_cast(wf::wl_surface_to_wayfire_view(surface->surface->resource))
                                      : nullptr);
        });
        wish->on_configure.connect(&surface->events.request_configure);
        wish->on_move.set_callback([this, surface](void*) { refuse_unless_pressed(surface); });
        wish->on_move.connect(&surface->events.request_move);
        wish->on_resize.set_callback([this, surface](void*) { refuse_unless_pressed(surface); });
        wish->on_resize.connect(&surface->events.request_resize);
        wish->on_destroy.set_callback([this, surface](void*) {
            // wlroots requires every listener gone by the time the window is.
            auto entry = wishes.find(surface);
            entry->second->on_configure.disconnect();
            entry->second->on_move.disconnect();
            entry->second->on_resize.disconnect();
            entry->second->on_destroy.disconnect();
            gone.push_back(std::move(entry->second));
            wishes.erase(entry);
            free_gone.run_once([this] { gone.clear(); });
        });
        wish->on_destroy.connect(&surface->events.destroy);
        wishes[surface] = std::move(wish);
    };

    wf::signal::connection_t<wf::view_mapped_signal> on_mapped = [this](wf::view_mapped_signal* event) {
        consider(wf::toplevel_cast(event->view));
    };

    // The screen holding the middle of what the window asked for, else its
    // own. X11's positions are the desktop's unless X11 programs are drawn
    // sharp; then they are in pixels of each screen, and its own is taken.
    wf::option_wrapper_t<bool> sharp_x11{"workarounds/force_xwayland_scaling"};

    wf::output_t* asked_screen(wayfire_toplevel_view view, const wf::geometry_t& asked)
    {
        if (!sharp_x11)
        {
            const wf::pointf_t middle{asked.x + asked.width / 2, asked.y + asked.height / 2};
            for (wf::output_t* output : wf::get_core().output_layout->get_outputs())
            {
                if (output->get_layout_geometry() & middle)
                {
                    return output;
                }
            }
        }
        return view->get_output();
    }

    // Whether the size asked for is at least the screen's, in X11's units.
    bool covers(wf::output_t* output, const wf::geometry_t& asked)
    {
        const wf::geometry_t screen = output->get_layout_geometry();
        const double units = sharp_x11 ? output->handle->scale : 1.0;
        return (asked.width >= std::floor(screen.width * units)) && (asked.height >= std::floor(screen.height * units));
    }

    void consider(wayfire_toplevel_view view)
    {
        wlr_surface* main = (view && view->is_mapped()) ? view->get_wlr_surface() : nullptr;
        wlr_xwayland_surface* surface = main ? wlr_xwayland_surface_try_from_wlr_surface(main) : nullptr;
        // Dialogs have a parent.
        if (!games || !surface || (surface->parent != nullptr) || !wishes.count(surface))
        {
            return;
        }
        wish_t& wish = *wishes[surface];
        wf::output_t* screen = asked_screen(view, wish.asked);
        if (screen == nullptr)
        {
            return;
        }

        // Full screen, by itself (Wine and Proton make it so) or by this
        // plugin: on the screen it asked for, not where the focus was.
        if (view->pending_fullscreen() && (screen != view->get_output()) && covers(screen, wish.asked))
        {
            wf::get_core().default_wm->fullscreen_request(view, screen, true);
            return;
        }

        // A frame means the window is meant to be moved about. A maximized
        // window without one (Steam draws its own) fills a screen without a
        // menu bar or dock, and is meant to stay a window.
        const bool maximized = surface->maximized_horz || surface->maximized_vert || view->pending_tiled_edges();
        const bool borderless_cover = (surface->decorations != 0) && !maximized && covers(screen, wish.asked);
        if (borderless_cover && !view->pending_fullscreen())
        {
            wf::get_core().default_wm->fullscreen_request(view, screen, true);
            wish.made_full_screen = true;
        }
        else if (!borderless_cover && wish.made_full_screen && view->pending_fullscreen())
        {
            wf::get_core().default_wm->fullscreen_request(view, view->get_output(), false);
            wish.made_full_screen = false;
        }
    }

  public:
    void init() override
    {
        wf::get_core().connect(&on_new_surface);
        wf::get_core().connect(&on_mapped);
        wf::get_core().connect(&on_button);
        wf::get_core().connect(&on_finger_down);
        wf::get_core().connect(&on_finger_up);
    }

    void fini() override
    {
        if (refusing_on != nullptr)
        {
            refusing_on->deactivate_plugin(&refusing);
            refusing_on = nullptr;
        }
        stop_refusing.disconnect();
        free_gone.disconnect();
        gone.clear();
        wishes.clear();
    }
};
#else
// Built without Xwayland: nothing to do.
class plugin_t : public wf::plugin_interface_t
{
  public:
    void init() override {}
};
#endif
} // namespace x11_windows

DECLARE_WAYFIRE_PLUGIN(x11_windows::plugin_t);
