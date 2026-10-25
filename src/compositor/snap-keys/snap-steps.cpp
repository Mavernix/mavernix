#include "snap-steps.hpp"

namespace snap_keys
{
namespace
{
// Where a window is across and down. "Whole" is the full width or height.
enum class across_t
{
    LEFT,
    WHOLE,
    RIGHT
};
enum class down_t
{
    TOP,
    WHOLE,
    BOTTOM
};

struct place_t
{
    // False for a floating window; then `across` and `down` mean nothing.
    bool tiled;
    across_t across;
    down_t down;
};

place_t place_of(uint32_t edges)
{
    if (edges == EDGES_ALL)
    {
        return {true, across_t::WHOLE, down_t::WHOLE};
    }

    // Tiled against the left edge but not the right one is on the left,
    // against both is across the whole width; and so on.
    const bool left = edges & EDGE_LEFT;
    const bool right = edges & EDGE_RIGHT;
    const bool top = edges & EDGE_TOP;
    const bool bottom = edges & EDGE_BOTTOM;

    // A half has three edges, a quarter two: one of each pair at least.
    if (!(left || right) || !(top || bottom))
    {
        return {false, across_t::WHOLE, down_t::WHOLE};
    }

    across_t across = left && right ? across_t::WHOLE : (left ? across_t::LEFT : across_t::RIGHT);
    down_t down = top && bottom ? down_t::WHOLE : (top ? down_t::TOP : down_t::BOTTOM);
    return {true, across, down};
}

uint32_t edges_of(across_t across, down_t down)
{
    uint32_t edges = 0;
    switch (across)
    {
    case across_t::LEFT:
        edges |= EDGE_LEFT;
        break;

    case across_t::RIGHT:
        edges |= EDGE_RIGHT;
        break;

    case across_t::WHOLE:
        edges |= EDGE_LEFT | EDGE_RIGHT;
        break;
    }

    switch (down)
    {
    case down_t::TOP:
        edges |= EDGE_TOP;
        break;

    case down_t::BOTTOM:
        edges |= EDGE_BOTTOM;
        break;

    case down_t::WHOLE:
        edges |= EDGE_TOP | EDGE_BOTTOM;
        break;
    }

    return edges;
}

snap_step tile(across_t across, down_t down)
{
    return {snap_kind::TILE, edges_of(across, down)};
}

const snap_step NOTHING = {snap_kind::NOTHING, 0};
const snap_step RESTORE = {snap_kind::RESTORE, 0};
const snap_step MINIMIZE = {snap_kind::MINIMIZE, 0};

// Left; Right is the same seen in a mirror (see next_snap_step).
snap_step step_left(const place_t& place)
{
    if (!place.tiled)
    {
        return tile(across_t::LEFT, down_t::WHOLE);
    }

    switch (place.across)
    {
    case across_t::LEFT:
        return NOTHING;

    case across_t::WHOLE:
        // The whole screen becomes the left half; an upper or lower half
        // becomes the quarter at its left, keeping its height.
        return tile(across_t::LEFT, place.down);

    case across_t::RIGHT:
        // A right half goes back where it came from; a right quarter
        // moves across to the left one.
        return place.down == down_t::WHOLE ? RESTORE : tile(across_t::LEFT, place.down);
    }
    return NOTHING;
}

snap_step step_up(const place_t& place)
{
    if (!place.tiled)
    {
        return tile(across_t::WHOLE, down_t::WHOLE);
    }

    switch (place.down)
    {
    case down_t::WHOLE:
        // A half becomes its upper quarter; the whole screen stays.
        return place.across == across_t::WHOLE ? NOTHING : tile(place.across, down_t::TOP);

    case down_t::TOP:
        return tile(across_t::WHOLE, down_t::WHOLE);

    case down_t::BOTTOM:
        // A lower quarter becomes its half; the lower half goes back.
        return place.across == across_t::WHOLE ? RESTORE : tile(place.across, down_t::WHOLE);
    }
    return NOTHING;
}

snap_step step_down(const place_t& place)
{
    if (!place.tiled)
    {
        return MINIMIZE;
    }

    switch (place.down)
    {
    case down_t::WHOLE:
        // A half becomes its lower quarter; the whole screen goes back.
        return place.across == across_t::WHOLE ? RESTORE : tile(place.across, down_t::BOTTOM);

    case down_t::TOP:
        // An upper quarter becomes its half; the upper half goes back.
        return place.across == across_t::WHOLE ? RESTORE : tile(place.across, down_t::WHOLE);

    case down_t::BOTTOM:
        return RESTORE;
    }
    return NOTHING;
}

across_t mirrored(across_t across)
{
    switch (across)
    {
    case across_t::LEFT:
        return across_t::RIGHT;

    case across_t::RIGHT:
        return across_t::LEFT;

    case across_t::WHOLE:
        break;
    }
    return across_t::WHOLE;
}

// Right is Left with left and right exchanged, before and after.
snap_step step_right(const place_t& place)
{
    place_t seen_mirrored = place;
    seen_mirrored.across = mirrored(place.across);
    snap_step step = step_left(seen_mirrored);
    if (step.kind == snap_kind::TILE)
    {
        const uint32_t kept = step.edges & (EDGE_TOP | EDGE_BOTTOM);
        const bool on_left = step.edges & EDGE_LEFT;
        const bool on_right = step.edges & EDGE_RIGHT;
        step.edges = kept | (on_left ? EDGE_RIGHT : 0) | (on_right ? EDGE_LEFT : 0);
    }
    return step;
}
} // namespace

snap_step next_snap_step(uint32_t current_edges, snap_direction direction)
{
    const place_t place = place_of(current_edges);
    switch (direction)
    {
    case snap_direction::LEFT:
        return step_left(place);

    case snap_direction::RIGHT:
        return step_right(place);

    case snap_direction::UP:
        return step_up(place);

    case snap_direction::DOWN:
        return step_down(place);
    }
    return NOTHING;
}
} // namespace snap_keys
