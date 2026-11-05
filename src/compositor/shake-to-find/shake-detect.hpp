// Tells a shake of the pointer, quickly back and forth, from ordinary
// movement. Free of Wayfire so tests/test-shake-to-find.cpp can check it.

#pragma once

#include <cstdint>

namespace shake_to_find
{
// How hard a shake must be, from 1 (the least) to 5. A turn counts once the
// pointer has gone turn_after_px each way, so the jitter of a hand that
// holds still is no shake; `turns` turns within SHAKE_WITHIN_MS start a
// shake.
struct needs_t
{
    double turn_after_px;
    int turns;
};
const needs_t LEAST_NEEDS = {40, 4};
// [input] shake_sensitivity; 3 when the config says nothing.
needs_t needs_for(int level);

// The values of the least that is needed, which a detector made without
// needs uses.
const double TURN_AFTER_PX = LEAST_NEEDS.turn_after_px;
const int TURNS_TO_SHAKE = LEAST_NEEDS.turns;
const int MOST_TURNS = 6;
const int64_t SHAKE_WITHIN_MS = 1000;
// ...which lasts while turns keep coming, at most this far apart. A pause
// as long forgets the turns so far.
const int64_t TURNS_APART_MS = 350;

class shake_detect_t
{
  public:
    explicit shake_detect_t(needs_t needs = LEAST_NEEDS);

    // The pointer moved by dx, dy pixels at now_ms. Only side to side counts.
    void move(double dx, double dy, int64_t now_ms);
    bool shaking(int64_t now_ms) const;
    // Forgets the movement so far, as when a button is pressed.
    void reset();

  private:
    void turned(int64_t now_ms);

    needs_t needs;

    // The way the pointer goes: -1 left, 1 right, 0 not yet known.
    int way = 0;
    // How far it has gone that way.
    double run_px = 0;
    // Whether the run before this one, the other way, was long enough.
    bool came_far = false;
    bool counted = false;
    int64_t last_move_ms = -1;

    // The times of the latest turns, oldest first.
    int64_t turns_ms[MOST_TURNS] = {};
    int turn_count = 0;
    bool shake = false;
};
} // namespace shake_to_find
