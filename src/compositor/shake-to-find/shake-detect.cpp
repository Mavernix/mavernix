#include "shake-detect.hpp"

#include <algorithm>
#include <cmath>

namespace shake_to_find
{
needs_t needs_for(int level)
{
    // Each step asks for a longer run each way, and the last two for more turns.
    static const needs_t levels[] = {{40, 4}, {60, 4}, {90, 5}, {130, 5}, {180, 6}};
    const int count = int(sizeof(levels) / sizeof(levels[0]));
    return levels[std::clamp(level, 1, count) - 1];
}

shake_detect_t::shake_detect_t(needs_t needs) : needs{needs.turn_after_px, std::clamp(needs.turns, 2, MOST_TURNS)} {}

void shake_detect_t::move(double dx, double, int64_t now_ms)
{
    if (last_move_ms >= 0 && now_ms - last_move_ms > TURNS_APART_MS)
    {
        reset();
    }
    last_move_ms = now_ms;
    // A shake that has ended starts again only with as many turns.
    if (shake && !shaking(now_ms))
    {
        shake = false;
    }
    if (dx == 0)
    {
        return;
    }

    const int now_way = dx > 0 ? 1 : -1;
    if (now_way != way)
    {
        came_far = way != 0 && run_px >= needs.turn_after_px;
        counted = false;
        way = now_way;
        run_px = 0;
    }
    run_px += std::abs(dx);

    // A turn counts when both runs, before and after it, were long ones.
    if (came_far && !counted && run_px >= needs.turn_after_px)
    {
        counted = true;
        turned(now_ms);
    }
}

void shake_detect_t::turned(int64_t now_ms)
{
    if (turn_count == needs.turns)
    {
        for (int i = 1; i < needs.turns; i++)
        {
            turns_ms[i - 1] = turns_ms[i];
        }
        turn_count--;
    }
    turns_ms[turn_count++] = now_ms;

    if (turn_count == needs.turns && now_ms - turns_ms[0] <= SHAKE_WITHIN_MS)
    {
        shake = true;
    }
}

bool shake_detect_t::shaking(int64_t now_ms) const
{
    return shake && now_ms - turns_ms[turn_count - 1] <= TURNS_APART_MS;
}

void shake_detect_t::reset()
{
    *this = shake_detect_t{needs};
}
} // namespace shake_to_find
