// The plugin "shake-to-find": shaking the pointer makes it big for a moment,
// so it's easy to find, as on OS X El Capitan.
//
// While a shake lasts (shake-detect.cpp) the arrow grows to GROWTH times its
// size, and when it ends the arrow shrinks back and the program under the
// pointer sets its own again. Wayfire's hide_cursor() keeps programs from
// changing the pointer meanwhile, and wlroots scales the theme's largest
// arrow: nothing is drawn here.
//
// Not while a button is held (a drag, or drawing), nor over a full-screen
// window, nor when a game has locked the pointer in place.

#include <algorithm>
#include <string>
#include <vector>

#include <wayfire/core.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/toplevel-view.hpp>
#include <wayfire/util.hpp>

#include "shake-detect.hpp"

namespace shake_to_find
{
// How many times its size the arrow grows.
const double GROWTH = 4;
const double GROW_MS = 250;
const double SHRINK_MS = 300;
const int FRAME_MS = 16;

class plugin_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<std::string> theme{"input/cursor_theme"};
    wf::option_wrapper_t<int> size{"input/cursor_size"};
    // How hard a shake must be, 1 to 5 (shake-detect.hpp).
    wf::option_wrapper_t<int> sensitivity{"shake-to-find/sensitivity"};

    // The theme at its largest, for the arrow.
    wlr_xcursor_manager* big_theme = nullptr;
    wlr_xcursor_image* arrow = nullptr;

    shake_detect_t detect;
    wf::pointf_t last_at;
    int buttons_down = 0;
    bool showing = false;
    // From 0, the usual size, to 1, the largest.
    double grown = 0;
    int64_t last_step_ms = 0;
    wf::wl_timer<true> steps;
    std::vector<wlr_output*> drawn_in_software;

    void load_arrow()
    {
        stop();
        if (big_theme)
        {
            wlr_xcursor_manager_destroy(big_theme);
        }
        arrow = nullptr;

        // The theme's size nearest to this is loaded, so its largest, and
        // sharp on screens scaled twice too.
        const std::string name = theme;
        const int largest = int(std::max(int(size), 1) * GROWTH * 2);
        big_theme = wlr_xcursor_manager_create(name == "default" ? nullptr : name.c_str(), largest);
        if (!big_theme || !wlr_xcursor_manager_load(big_theme, 1))
        {
            return;
        }
        wlr_xcursor* found = wlr_xcursor_manager_get_xcursor(big_theme, "default", 1);
        if (!found)
        {
            found = wlr_xcursor_manager_get_xcursor(big_theme, "left_ptr", 1);
        }
        if (found && found->image_count > 0)
        {
            arrow = found->images[0];
        }
    }

    bool may_grow()
    {
        auto toplevel = wf::toplevel_cast(wf::get_core().get_cursor_focus_view());
        return !(toplevel && toplevel->pending_fullscreen());
    }

    void start()
    {
        showing = true;
        last_step_ms = wf::get_current_time();
        wf::get_core().hide_cursor();
        // A hardware cursor plane can cut off an image drawn larger than it
        // is, so the screens draw the pointer themselves while it's big.
        for (wf::output_t* output : wf::get_core().output_layout->get_outputs())
        {
            wlr_output_lock_software_cursors(output->handle, true);
            drawn_in_software.push_back(output->handle);
        }
        step();
        steps.set_timeout(FRAME_MS, [this] { return step(); });
    }

    // One frame of growing or shrinking; false once the arrow is back.
    bool step()
    {
        const int64_t now = wf::get_current_time();
        const double elapsed = double(now - last_step_ms);
        last_step_ms = now;
        const bool shaking = detect.shaking(now);
        grown = std::clamp(grown + (shaking ? elapsed / GROW_MS : -elapsed / SHRINK_MS), 0.0, 1.0);
        if (!shaking && grown == 0)
        {
            stop();
            return false;
        }

        // Eased in and out.
        const double eased = grown * grown * (3 - 2 * grown);
        const double drawn_size = std::max(int(size), 1) * (1 + (GROWTH - 1) * eased);
        // wlroots draws the buffer at its size divided by the scale.
        const float scale = float(arrow->width / drawn_size);
        wlr_cursor_set_buffer(wf::get_core().get_wlr_cursor(), wlr_xcursor_image_get_buffer(arrow),
            int(arrow->hotspot_x / scale), int(arrow->hotspot_y / scale), scale);
        return true;
    }

    void stop()
    {
        if (!showing)
        {
            return;
        }
        showing = false;
        grown = 0;
        steps.disconnect();
        for (wlr_output* output : drawn_in_software)
        {
            wlr_output_lock_software_cursors(output, false);
        }
        drawn_in_software.clear();
        // Back to the arrow...
        wf::get_core().unhide_cursor();

        // ...and the program under the pointer sets its own again when the
        // pointer enters it anew. Not in the middle of a click.
        wlr_seat* seat = wf::get_core().get_current_seat();
        wlr_surface* surface = seat->pointer_state.focused_surface;
        if (surface && buttons_down == 0)
        {
            const double sx = seat->pointer_state.sx;
            const double sy = seat->pointer_state.sy;
            wlr_seat_pointer_notify_clear_focus(seat);
            wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
        }
    }

    wf::signal::connection_t<wf::post_input_event_signal<wlr_pointer_motion_event>> on_motion =
        [this](wf::post_input_event_signal<wlr_pointer_motion_event>*) {
        if (buttons_down > 0 || !arrow)
        {
            return;
        }
        // How far the pointer went, rather than the mouse: a pointer a game
        // has locked stays put however the mouse shakes.
        const wf::pointf_t at = wf::get_core().get_cursor_position();
        const double dx = at.x - last_at.x;
        last_at = at;
        const int64_t now = wf::get_current_time();
        detect.move(dx, 0, now);
        if (!showing && detect.shaking(now) && may_grow())
        {
            start();
        }
    };

    wf::signal::connection_t<wf::post_input_event_signal<wlr_pointer_button_event>> on_button =
        [this](wf::post_input_event_signal<wlr_pointer_button_event>* signal) {
        if (signal->event->state == WL_POINTER_BUTTON_STATE_PRESSED)
        {
            buttons_down++;
            // A shake under way ends, and the arrow shrinks back.
            detect.reset();
        }
        else
        {
            buttons_down = std::max(buttons_down - 1, 0);
        }
    };

    // The software cursors were asked of this screen's wlr_output.
    wf::signal::connection_t<wf::output_pre_remove_signal> on_output_leaving = [this](wf::output_pre_remove_signal*) {
        stop();
    };

  public:
    void init() override
    {
        load_arrow();
        theme.set_callback([this] { load_arrow(); });
        size.set_callback([this] { load_arrow(); });
        detect = shake_detect_t{needs_for(sensitivity)};
        sensitivity.set_callback([this] { detect = shake_detect_t{needs_for(sensitivity)}; });
        last_at = wf::get_core().get_cursor_position();
        wf::get_core().connect(&on_motion);
        wf::get_core().connect(&on_button);
        wf::get_core().output_layout->connect(&on_output_leaving);
    }

    void fini() override
    {
        stop();
        if (big_theme)
        {
            wlr_xcursor_manager_destroy(big_theme);
            big_theme = nullptr;
        }
    }
};
} // namespace shake_to_find

DECLARE_WAYFIRE_PLUGIN(shake_to_find::plugin_t);
