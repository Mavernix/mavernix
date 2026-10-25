// Tells what a movement of several fingers on a touchpad is: a swipe in one
// of four directions, a pinch, or nothing yet. Needs nothing of Wayfire.

#pragma once

#include <string>

namespace touchpad_gestures
{
enum class gesture_t
{
    NONE,
    SWIPE_UP,
    SWIPE_DOWN,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    PINCH_IN,
    PINCH_OUT,
};

/**
 * `right` and `down`: how far the fingers' middle has moved (negative for
 * left and up). A swipe counts once it has gone `least_distance` along one
 * direction and less far along the other.
 */
gesture_t read_swipe(double right, double down, double least_distance);

/**
 * `spread`: 1 when unchanged. A pinch counts once it has shrunk by the share
 * `least_change`, or grown by the inverse of that.
 */
gesture_t read_pinch(double spread, double least_change);

/** "swipe_up_3". */
std::string option_name(gesture_t gesture, int fingers);
} // namespace touchpad_gestures
