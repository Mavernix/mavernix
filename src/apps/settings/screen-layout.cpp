#include "screen-layout.hpp"

#include <QtGlobal>

#include <algorithm>
#include <limits>

// How much of an edge two screens of these lengths must share.
static int needed_share(int one, int other)
{
    return std::min({least_shared_edge_px, one, other});
}

// Along an edge from `start` that is `length` long, where a side `size`
// long goes: as near `wanted` as it may while sharing enough, and lined up
// with either end when that is close.
static int along(int wanted, int size, int start, int length, int line_up)
{
    const int share = needed_share(size, length);
    int at = std::clamp(wanted, start - size + share, start + length - share);
    for (const int end : {start, start + length - size})
    {
        if (std::abs(at - end) <= line_up)
        {
            at = end;
        }
    }
    return at;
}

QPoint snapped(const QRect& moved, const QList<QRect>& others, int line_up)
{
    QPoint best = moved.topLeft();
    qint64 best_distance = std::numeric_limits<qint64>::max();
    const int width = moved.width();
    const int height = moved.height();
    for (const QRect& other : others)
    {
        const int y = along(moved.y(), height, other.y(), other.height(), line_up);
        const int x = along(moved.x(), width, other.x(), other.width(), line_up);
        const QPoint places[] = {
            {other.x() + other.width(), y},
            {other.x() - width, y},
            {x, other.y() + other.height()},
            {x, other.y() - height},
        };
        for (const QPoint& place : places)
        {
            const QRect there(place, moved.size());
            if (std::any_of(others.begin(), others.end(), [&](const QRect& one) { return one.intersects(there); }))
            {
                continue;
            }
            const qint64 dx = place.x() - moved.x();
            const qint64 dy = place.y() - moved.y();
            if (dx * dx + dy * dy < best_distance)
            {
                best_distance = dx * dx + dy * dy;
                best = place;
            }
        }
    }
    return best;
}

bool touching(const QRect& one, const QRect& other)
{
    const bool side_by_side = one.x() + one.width() == other.x() || other.x() + other.width() == one.x();
    const bool stacked = one.y() + one.height() == other.y() || other.y() + other.height() == one.y();
    if (side_by_side)
    {
        const int shared = std::min(one.y() + one.height(), other.y() + other.height()) - std::max(one.y(), other.y());
        return shared >= needed_share(one.height(), other.height());
    }
    if (stacked)
    {
        const int shared = std::min(one.x() + one.width(), other.x() + other.width()) - std::max(one.x(), other.x());
        return shared >= needed_share(one.width(), other.width());
    }
    return false;
}

QMap<QString, QPoint> positions(const QMap<QString, QRect>& screens, const QString& main)
{
    const QPoint origin = screens.value(main).topLeft();
    QMap<QString, QPoint> at;
    for (auto screen = screens.constBegin(); screen != screens.constEnd(); ++screen)
    {
        at.insert(screen.key(), screen.value().topLeft() - origin);
    }
    return at;
}
