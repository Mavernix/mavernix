#include "seam-math.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace shared_edges
{
namespace
{
// A window seen along the direction the seam moves in ("along") and
// across it. For a vertical seam along is x and across is y.
struct view_along_t
{
    int along_start;
    int along_length;
    int across_start;
    int across_length;
    int least_along;
    int most_along;
};

view_along_t seen_along(const window_t& window, bool vertical)
{
    if (vertical)
    {
        return {window.box.x, window.box.width, window.box.y, window.box.height, window.min_width, window.max_width};
    }

    return {window.box.y, window.box.height, window.box.x, window.box.width, window.min_height, window.max_height};
}

bool overlap_across(const view_along_t& a, const view_along_t& b)
{
    return (a.across_start < b.across_start + b.across_length) && (b.across_start < a.across_start + a.across_length);
}

bool contains(const std::vector<std::size_t>& list, std::size_t index)
{
    return std::find(list.begin(), list.end(), index) != list.end();
}
} // namespace

std::optional<seam_t> find_seam(
    const std::vector<window_t>& windows, std::size_t dragged, side_t side, int tolerance_px)
{
    seam_t seam;
    seam.vertical = (side == side_t::LEFT) || (side == side_t::RIGHT);
    const bool dragged_is_before = (side == side_t::RIGHT) || (side == side_t::BOTTOM);

    const view_along_t dragged_view = seen_along(windows.at(dragged), seam.vertical);
    const int seam_position =
        dragged_is_before ? dragged_view.along_start + dragged_view.along_length : dragged_view.along_start;

    // Every window with an edge on the seam, on one side or the other.
    std::vector<std::size_t> on_seam;
    for (std::size_t index = 0; index < windows.size(); index++)
    {
        const view_along_t view = seen_along(windows[index], seam.vertical);
        const bool ends_on_seam = std::abs(view.along_start + view.along_length - seam_position) <= tolerance_px;
        const bool starts_on_seam = std::abs(view.along_start - seam_position) <= tolerance_px;
        if (index == dragged || ends_on_seam || starts_on_seam)
        {
            on_seam.push_back(index);
        }
    }

    // Start from the dragged window and add windows that touch one already
    // taken along the seam, until none more do. That takes in two squares
    // stacked beside a tall window, and leaves out a window further along
    // the same line that has a gap between it and the others.
    std::vector<std::size_t> joined = {dragged};
    bool grew = true;
    while (grew)
    {
        grew = false;
        for (std::size_t candidate : on_seam)
        {
            if (contains(joined, candidate))
            {
                continue;
            }

            const view_along_t candidate_view = seen_along(windows[candidate], seam.vertical);
            for (std::size_t member : joined)
            {
                if (overlap_across(candidate_view, seen_along(windows[member], seam.vertical)))
                {
                    joined.push_back(candidate);
                    grew = true;
                    break;
                }
            }
        }
    }

    for (std::size_t index : joined)
    {
        const view_along_t view = seen_along(windows[index], seam.vertical);
        const bool ends_on_seam = std::abs(view.along_start + view.along_length - seam_position) <= tolerance_px;
        (ends_on_seam ? seam.before : seam.after).push_back(index);
    }

    // A seam has two sides.
    if (seam.before.empty() || seam.after.empty())
    {
        return std::nullopt;
    }

    return seam;
}

int limit_move(const std::vector<window_t>& windows, const seam_t& seam, int wanted_px, int floor_px)
{
    // The seam may move by an amount between `lowest` and `highest`.
    int lowest = INT_MIN;
    int highest = INT_MAX;

    for (std::size_t index : seam.before)
    {
        // Moving the seam by m makes a window before it longer by m.
        const view_along_t view = seen_along(windows[index], seam.vertical);
        lowest = std::max(lowest, std::max(view.least_along, floor_px) - view.along_length);
        if (view.most_along > 0)
        {
            highest = std::min(highest, view.most_along - view.along_length);
        }
    }

    for (std::size_t index : seam.after)
    {
        // ... and a window after it shorter by m.
        const view_along_t view = seen_along(windows[index], seam.vertical);
        highest = std::min(highest, view.along_length - std::max(view.least_along, floor_px));
        if (view.most_along > 0)
        {
            lowest = std::max(lowest, view.along_length - view.most_along);
        }
    }

    if (lowest > highest)
    {
        // The windows' limits do not leave any room: do not move at all.
        return 0;
    }

    return std::clamp(wanted_px, lowest, highest);
}

box_t moved_box(const window_t& window, const seam_t& seam, std::size_t index, int move_px)
{
    box_t box = window.box;
    if (contains(seam.before, index))
    {
        (seam.vertical ? box.width : box.height) += move_px;
    }
    else if (contains(seam.after, index))
    {
        (seam.vertical ? box.x : box.y) += move_px;
        (seam.vertical ? box.width : box.height) -= move_px;
    }

    return box;
}
} // namespace shared_edges
