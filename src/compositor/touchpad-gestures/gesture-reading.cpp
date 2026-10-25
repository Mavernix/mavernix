#include "gesture-reading.hpp"

#include <cmath>

namespace touchpad_gestures
{
gesture_t read_swipe(double right, double down, double least_distance)
{
    const bool sideways = std::abs(right) > std::abs(down);
    const double travelled = sideways ? std::abs(right) : std::abs(down);
    if (travelled < least_distance)
    {
        return gesture_t::NONE;
    }

    if (sideways)
    {
        return right > 0 ? gesture_t::SWIPE_RIGHT : gesture_t::SWIPE_LEFT;
    }

    return down > 0 ? gesture_t::SWIPE_DOWN : gesture_t::SWIPE_UP;
}

gesture_t read_pinch(double spread, double least_change)
{
    const double smallest = 1.0 - least_change;
    if (spread <= smallest)
    {
        return gesture_t::PINCH_IN;
    }

    if (spread >= 1.0 / smallest)
    {
        return gesture_t::PINCH_OUT;
    }

    return gesture_t::NONE;
}

std::string option_name(gesture_t gesture, int fingers)
{
    std::string name;
    switch (gesture)
    {
    case gesture_t::SWIPE_UP:
        name = "swipe_up";
        break;

    case gesture_t::SWIPE_DOWN:
        name = "swipe_down";
        break;

    case gesture_t::SWIPE_LEFT:
        name = "swipe_left";
        break;

    case gesture_t::SWIPE_RIGHT:
        name = "swipe_right";
        break;

    case gesture_t::PINCH_IN:
        name = "pinch_in";
        break;

    case gesture_t::PINCH_OUT:
        name = "pinch_out";
        break;

    case gesture_t::NONE:
        return "";
    }

    return name + "_" + std::to_string(fingers);
}
} // namespace touchpad_gestures
