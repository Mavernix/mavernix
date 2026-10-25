// The plugin "touchpad-gestures": runs a command when three or four
// fingers swipe or pinch on a touchpad (Wayfire binds touchpad swipes only
// to vswipe). A gesture fires once, as soon as the fingers have gone far
// enough. Two fingers are left to programs for scrolling and zooming.

#include <map>

#include <wayfire/core.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/signal-definitions.hpp>

#include "gesture-reading.hpp"

namespace touchpad_gestures
{
const std::string PLUGIN_NAME = "touchpad-gestures";
const int FEWEST_FINGERS = 3;
const int MOST_FINGERS = 4;

class plugin_t : public wf::plugin_interface_t
{
    wf::option_wrapper_t<double> swipe_distance{PLUGIN_NAME + "/swipe_distance"};
    wf::option_wrapper_t<double> pinch_change{PLUGIN_NAME + "/pinch_change"};
    // By option name ("swipe_up_3").
    std::map<std::string, wf::option_wrapper_t<std::string>> commands;

    // The gesture under way.
    int fingers = 0;
    double travelled_right = 0;
    double travelled_down = 0;
    bool acted = false;

    void begin(uint32_t finger_count)
    {
        fingers = int(finger_count);
        travelled_right = 0;
        travelled_down = 0;
        acted = false;
    }

    bool listens() const { return !acted && (fingers >= FEWEST_FINGERS) && (fingers <= MOST_FINGERS); }

    void act_on(gesture_t gesture)
    {
        if (gesture == gesture_t::NONE)
        {
            return;
        }

        acted = true;
        const auto command = commands.find(option_name(gesture, fingers));
        if ((command != commands.end()) && !std::string(command->second).empty())
        {
            wf::get_core().run(command->second);
        }
    }

    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_swipe_begin_event>> on_swipe_begin =
        [=](wf::input_event_signal<wlr_pointer_swipe_begin_event>* signal) { begin(signal->event->fingers); };

    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_swipe_update_event>> on_swipe_update =
        [=](wf::input_event_signal<wlr_pointer_swipe_update_event>* signal) {
        if (!listens())
        {
            return;
        }

        travelled_right += signal->event->dx;
        travelled_down += signal->event->dy;
        act_on(read_swipe(travelled_right, travelled_down, swipe_distance));
    };

    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_pinch_begin_event>> on_pinch_begin =
        [=](wf::input_event_signal<wlr_pointer_pinch_begin_event>* signal) { begin(signal->event->fingers); };

    wf::signal::connection_t<wf::input_event_signal<wlr_pointer_pinch_update_event>> on_pinch_update =
        [=](wf::input_event_signal<wlr_pointer_pinch_update_event>* signal) {
        if (!listens())
        {
            return;
        }

        act_on(read_pinch(signal->event->scale, pinch_change));
    };

  public:
    void init() override
    {
        const gesture_t gestures[] = {
            gesture_t::SWIPE_UP,
            gesture_t::SWIPE_DOWN,
            gesture_t::SWIPE_LEFT,
            gesture_t::SWIPE_RIGHT,
            gesture_t::PINCH_IN,
            gesture_t::PINCH_OUT,
        };
        for (gesture_t gesture : gestures)
        {
            for (int finger_count = FEWEST_FINGERS; finger_count <= MOST_FINGERS; finger_count++)
            {
                const std::string name = option_name(gesture, finger_count);
                commands[name].load_option(PLUGIN_NAME + "/" + name);
            }
        }

        wf::get_core().connect(&on_swipe_begin);
        wf::get_core().connect(&on_swipe_update);
        wf::get_core().connect(&on_pinch_begin);
        wf::get_core().connect(&on_pinch_update);
    }

    void fini() override
    {
        on_swipe_begin.disconnect();
        on_swipe_update.disconnect();
        on_pinch_begin.disconnect();
        on_pinch_update.disconnect();
    }
};
} // namespace touchpad_gestures

DECLARE_WAYFIRE_PLUGIN(touchpad_gestures::plugin_t);
