// The plugin "borderless-full-screen": an X11 window without a frame that
// asks to cover a whole screen is made full screen there. Games do this for
// "borderless" or "windowed full screen" (Wine and Proton among them)
// instead of asking to be full screen. Wayfire otherwise fits the window
// into the room between the menu bar and the dock, cutting the game short.
// KWin and Mutter treat such windows the same way.
//
// Wayfire changes the size a window asks for before anyone else sees it, so
// the plugin notes each X11 window's own wish: its size when made, and every
// resize it asks for afterwards. Option "enabled".

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

namespace borderless_full_screen
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
    wf::wl_listener_wrapper on_configure;
    wf::wl_listener_wrapper on_destroy;
};

class plugin_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<bool> enabled{"borderless-full-screen/enabled"};
    std::map<wlr_xwayland_surface*, std::unique_ptr<wish_t>> wishes;
    // A wish is freed only after its window's destroy listener has run.
    std::vector<std::unique_ptr<wish_t>> gone;
    wf::wl_idle_call free_gone;

    wf::signal::connection_t<wf::new_xwayland_surface_signal> on_new_surface =
        [this](wf::new_xwayland_surface_signal* event) {
        wlr_xwayland_surface* surface = event->surface;
        auto wish = std::make_unique<wish_t>();
        wish->asked = box(surface->x, surface->y, surface->width, surface->height);
        wish->on_configure.set_callback([wish = wish.get()](void* data) {
            const auto* asked = static_cast<wlr_xwayland_surface_configure_event*>(data);
            wish->asked = box(asked->x, asked->y, asked->width, asked->height);
        });
        wish->on_configure.connect(&surface->events.request_configure);
        wish->on_destroy.set_callback([this, surface](void*) {
            // wlroots requires every listener gone by the time the window is.
            auto entry = wishes.find(surface);
            entry->second->on_configure.disconnect();
            entry->second->on_destroy.disconnect();
            gone.push_back(std::move(entry->second));
            wishes.erase(entry);
            free_gone.run_once([this] { gone.clear(); });
        });
        wish->on_destroy.connect(&surface->events.destroy);
        wishes[surface] = std::move(wish);
    };

    wf::signal::connection_t<wf::view_mapped_signal> on_mapped = [this](wf::view_mapped_signal* event) {
        if (!enabled)
        {
            return;
        }
        auto view = wf::toplevel_cast(event->view);
        wlr_surface* main = view ? view->get_wlr_surface() : nullptr;
        wlr_xwayland_surface* surface = main ? wlr_xwayland_surface_try_from_wlr_surface(main) : nullptr;
        // Dialogs have a parent; a frame means the window is meant to be moved.
        if (!surface || (surface->parent != nullptr) || (surface->decorations == 0) || view->pending_fullscreen() ||
            !wishes.count(surface))
        {
            return;
        }

        const wf::geometry_t asked = wishes[surface]->asked;
        for (wf::output_t* output : wf::get_core().output_layout->get_outputs())
        {
            const wf::geometry_t screen = output->get_layout_geometry();
            const bool covers = (asked.x <= screen.x) && (asked.y <= screen.y) &&
                                (asked.x + asked.width >= screen.x + screen.width) &&
                                (asked.y + asked.height >= screen.y + screen.height);
            if (covers)
            {
                wf::get_core().default_wm->fullscreen_request(view, output, true);
                return;
            }
        }
    };

  public:
    void init() override
    {
        wf::get_core().connect(&on_new_surface);
        wf::get_core().connect(&on_mapped);
    }

    void fini() override
    {
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
} // namespace borderless_full_screen

DECLARE_WAYFIRE_PLUGIN(borderless_full_screen::plugin_t);
