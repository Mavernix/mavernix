// Tells a shake of the pointer, quickly back and forth, from ordinary
// movement. Free of Wayfire so tests/test-shake-to-find.cpp can check it.

#pragma once

#include <cstdint>

namespace shake_to_find
{
// A turn counts once the pointer has gone this far each way, so the jitter
// of a hand that holds still is no shake.
const double TURN_AFTER_PX = 40;
// This many turns within SHAKE_WITHIN_MS start a shake...
const int TURNS_TO_SHAKE = 4;
const int64_t SHAKE_WITHIN_MS = 1000;
// ...which lasts while turns keep coming, at most this far apart. A pause
// as long forgets the turns so far.
const int64_t TURNS_APART_MS = 350;

class shake_detect_t
{
  public:
    // The pointer moved by dx, dy pixels at now_ms. Only side to side counts.
    void move(double dx, double dy, int64_t now_ms);
    bool shaking(int64_t now_ms) const;
    // Forgets the movement so far, as when a button is pressed.
    void reset();

  private:
    void turned(int64_t now_ms);

    // The way the pointer goes: -1 left, 1 right, 0 not yet known.
    int way = 0;
    // How far it has gone that way.
    double run_px = 0;
    // Whether the run before this one, the other way, was long enough.
    bool came_far = false;
    bool counted = false;
    int64_t last_move_ms = -1;

    // The times of the latest turns, oldest first.
    int64_t turns_ms[TURNS_TO_SHAKE] = {};
    int turn_count = 0;
    bool shake = false;
};
} // namespace shake_to_find
