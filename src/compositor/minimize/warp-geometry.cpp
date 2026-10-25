#include "warp-geometry.hpp"

#include <algorithm>
#include <cmath>

namespace warp
{
namespace
{
double clamp_share(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double mix(double from, double to, double share)
{
    return from + (to - from) * share;
}

/**
 * How long a movement over `distance` pixels takes, in relative units; the
 * whole effect is later fitted into the theme's duration.
 */
double movement_length(double distance)
{
    const double units_per_step = 50.0;
    return units_per_step * std::trunc(std::log(1.0 + 0.5 * std::abs(distance)));
}
}

double ease(double share)
{
    return 0.5 - 0.5 * std::cos(M_PI * clamp_share(share));
}

std::vector<row_t> genie_rows(const box_t& window, const box_t& icon, double progress,
    double row_step)
{
    // Two overlapping movements: a funnel from the window's top down to the
    // icon deepens, and the window slides down through it. The slide is
    // delayed so the window's lower edge reaches the icon just as the funnel
    // is at full depth.
    const double travel = icon.y - window.y;
    const double left_shift  = icon.x - window.x;
    const double right_shift = (icon.x + icon.width) - (window.x + window.width);
    if (travel <= 0)
    {
        // The funnel only works for an icon below the window.
        return scale_rows(window, icon, progress);
    }

    // A window that reaches below the icon's top is first lifted clear of it.
    const double overlap = std::min(0.0, icon.y - (window.y + window.height));

    const double funnel_length = movement_length(
        std::max(std::abs(2.0 * overlap), (std::abs(left_shift) + std::abs(right_shift)) / 8.0));
    const double slide_length = movement_length(travel);

    // When the window's lower edge reaches the icon: the inverse of ease().
    const double gap_share = clamp_share(std::abs(travel - window.height) / travel);
    const double arrival = slide_length * std::acos(1.0 - 2.0 * gap_share) / M_PI;

    const double funnel_time = std::max(funnel_length, arrival);
    const double slide_delay = std::max(0.0, funnel_length - arrival);
    const double total_time  = slide_length + slide_delay;
    const double now = clamp_share(progress) * total_time;

    const double funnel_depth = (funnel_time > 0) ? ease(now / funnel_time) : 1.0;
    const double slid = (slide_length > 0) ? ease((now - slide_delay) / slide_length) : 1.0;

    // No shift at the funnel's top, the full shift at the icon, eased between.
    const double funnel_top = window.y + overlap * funnel_depth;
    const double funnel_height = std::trunc(travel + 0.5);
    auto shift_at = [&] (double y, double full_shift)
    {
        return full_shift * funnel_depth * ease((y - funnel_top) / funnel_height);
    };

    // Whatever would reach below the icon is cut off.
    const double picture_top = window.y + overlap * funnel_depth + travel * slid;
    const double visible_bottom = std::min(picture_top + window.height, icon.y + icon.height);

    std::vector<row_t> rows;
    if (picture_top >= visible_bottom)
    {
        return rows;
    }

    auto add_row = [&] (double y)
    {
        rows.push_back({
            y,
            window.x + shift_at(y, left_shift),
            window.x + window.width + shift_at(y, right_shift),
            (y - picture_top) / window.height,
        });
    };

    for (double y = picture_top; y < visible_bottom; y += row_step)
    {
        add_row(y);
    }
    add_row(visible_bottom);
    return rows;
}

std::vector<row_t> scale_rows(const box_t& window, const box_t& icon, double progress)
{
    const double moved = ease(progress);
    const double top    = mix(window.y, icon.y, moved);
    const double bottom = mix(window.y + window.height, icon.y + icon.height, moved);
    const double left   = mix(window.x, icon.x, moved);
    const double right  = mix(window.x + window.width, icon.x + icon.width, moved);

    return {
        {top, left, right, 0.0},
        {bottom, left, right, 1.0},
    };
}
}
