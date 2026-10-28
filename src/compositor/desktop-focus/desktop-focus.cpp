// The plugin "desktop-focus": a click on the desktop leaves no window
// active, so its title bar greys and the menu bar shows the desktop's menus.
//
// Wayfire keeps the last window active while a shell surface has the
// keyboard ("workarounds/keep_last_toplevel_activated"), which the menu bar's
// menus and the dock need. The desktop is the one surface that should not:
// when a surface in the background layer takes the keyboard, the active
// window is let go. The keyboard stays with the desktop, which holds it.

#include <wayfire/core.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/seat.hpp>
#include <wayfire/view-helpers.hpp>
#include <wayfire/view.hpp>

namespace desktop_focus
{
class plugin_t : public wf::plugin_interface_t
{
    wf::signal::connection_t<wf::keyboard_focus_changed_signal> on_focus_changed =
        [](wf::keyboard_focus_changed_signal* event) {
        wayfire_view view = wf::node_to_view(event->new_focus);
        if (!view || (wf::get_view_layer(view) != wf::scene::layer::BACKGROUND))
        {
            return;
        }

        // Clears the active window only; the keyboard stays where it is.
        wf::get_core().seat->focus_view(nullptr);
    };

  public:
    void init() override { wf::get_core().connect(&on_focus_changed); }

    void fini() override { on_focus_changed.disconnect(); }
};
} // namespace desktop_focus

DECLARE_WAYFIRE_PLUGIN(desktop_focus::plugin_t);
