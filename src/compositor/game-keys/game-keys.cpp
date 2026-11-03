// The plugin "game-keys": lets a function key that is one of the desktop's
// shortcuts (Sea Lion's F3, F4 and F11) through to a game instead.
//
// A press of one of the keys in "keys", with no modifier held, reaches the
// focused window rather than the compositor's bindings when that window is
// an X11 program (Minecraft, Wine and Proton games) or full screen. Over
// any other window, or the desktop, the key does what the shortcut says.
//
// For the length of that one key event, the bindings are switched off
// (they are counted, as shortcut-capture's are), so core finds no binding
// and hands the key to the window, as it would any other.

#include <sstream>
#include <string>

#include <linux/input-event-codes.h>

#include <wayfire/bindings-repository.hpp>
#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/seat.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/view.hpp>

namespace game_keys
{
// Our wlroots keycodes are the kernel's, as in the config ("KEY_F3").
static int function_key_code(const std::string& name)
{
    static const int codes[] = {
        KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12};
    for (int number = 1; number <= 12; number++)
    {
        if (name == "KEY_F" + std::to_string(number))
        {
            return codes[number - 1];
        }
    }
    return -1;
}

static bool is_game(wayfire_view view)
{
    if (!view)
    {
        return false;
    }
    if (auto toplevel = wf::toplevel_cast(view); toplevel && toplevel->pending_fullscreen())
    {
        return true;
    }
#if WF_HAS_XWAYLAND
    wlr_surface* surface = view->get_wlr_surface();
    return surface && wlr_xwayland_surface_try_from_wlr_surface(surface);
#else
    return false;
#endif
}

class plugin_t : public wf::plugin_interface_t
{
    // "KEY_F3 KEY_F4 KEY_F11", written by make_wayfire_config.py.
    wf::option_wrapper_t<std::string> keys{"game-keys/keys"};
    bool switched_off = false;

    bool is_listed(uint32_t code)
    {
        std::istringstream names{std::string(keys)};
        std::string name;
        while (names >> name)
        {
            if (function_key_code(name) == (int)code)
            {
                return true;
            }
        }
        return false;
    }

    wf::signal::connection_t<wf::input_event_signal<wlr_keyboard_key_event>> on_key =
        [=](wf::input_event_signal<wlr_keyboard_key_event>* signal) {
        if (signal->event->state != WL_KEYBOARD_KEY_STATE_PRESSED || !is_listed(signal->event->keycode))
        {
            return;
        }
        const uint32_t held = wf::get_core().seat->get_keyboard_modifiers() &
                              (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO);
        // During a grab (the overview open) the focus is no window, and the key is the compositor's.
        if (held == 0 && is_game(wf::node_to_view(wf::get_core().seat->get_active_node())))
        {
            wf::get_core().bindings->set_enabled(false);
            switched_off = true;
        }
    };

    // Core sends this after every key event, whichever way it went.
    wf::signal::connection_t<wf::post_input_event_signal<wlr_keyboard_key_event>> on_key_done =
        [=](wf::post_input_event_signal<wlr_keyboard_key_event>*) { switch_on(); };

    void switch_on()
    {
        if (switched_off)
        {
            wf::get_core().bindings->set_enabled(true);
            switched_off = false;
        }
    }

  public:
    void init() override
    {
        wf::get_core().connect(&on_key);
        wf::get_core().connect(&on_key_done);
    }

    void fini() override
    {
        on_key.disconnect();
        on_key_done.disconnect();
        switch_on();
    }
};
} // namespace game_keys

DECLARE_WAYFIRE_PLUGIN(game_keys::plugin_t);
