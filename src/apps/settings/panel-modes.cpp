#include "panel-modes.hpp"

#include <QSize>

#include <cstdlib>

// Common sizes of the usual shapes (16:10, 16:9, 3:2, 4:3), the ones
// programs and games expect. Only those of the panel's own shape are
// offered: any other is drawn with black bars or out of proportion.
static const QSize common_sizes[] = {
    {3840, 2400}, {3840, 2160}, {3200, 2000}, {3200, 1800}, {3000, 2000}, {2880, 1920}, {2880, 1800},
    {2880, 1620}, {2560, 1600}, {2560, 1440}, {2400, 1600}, {2304, 1440}, {2256, 1504}, {2048, 1536},
    {2048, 1280}, {2048, 1152}, {1920, 1440}, {1920, 1280}, {1920, 1200}, {1920, 1080}, {1680, 1120},
    {1680, 1050}, {1600, 1200}, {1600, 900},  {1440, 960},  {1440, 900},  {1400, 1050}, {1280, 960},
    {1280, 800},  {1280, 720},  {1024, 768},
};

// Within half a percent: 1920 × 1200 and 2560 × 1600 are one shape.
static bool same_shape(const QSize& one, const QSize& other)
{
    const qint64 across = qint64(one.width()) * other.height();
    const qint64 down = qint64(other.width()) * one.height();
    return std::llabs(across - down) * 200 <= down;
}

bool is_built_in(const QString& name)
{
    return name.startsWith(QLatin1String("eDP-")) || name.startsWith(QLatin1String("LVDS-"))
           || name.startsWith(QLatin1String("DSI-"));
}

QList<ScreenMode> offered_modes(const QString& name, const ScreenMode& current, const QList<ScreenMode>& reported)
{
    QList<ScreenMode> modes = reported;
    // A custom mode's rate comes back from its timings a hair off the one asked.
    const auto has = [&modes](int width, int height, int refresh) {
        for (const ScreenMode& mode : modes)
        {
            if (mode.width == width && mode.height == height && std::abs(mode.refresh - refresh) <= same_rate_millihertz)
            {
                return true;
            }
        }
        return false;
    };

    // A screen that names more than one size has its driver's own list.
    bool one_size = !reported.isEmpty();
    for (const ScreenMode& mode : reported)
    {
        one_size = one_size && mode.width == reported.first().width && mode.height == reported.first().height;
    }
    if (one_size && is_built_in(name))
    {
        const QSize panel(reported.first().width, reported.first().height);
        // A panel built upright, as a tablet's, has the same sizes on end.
        const bool upright = panel.height() > panel.width();
        for (QSize size : common_sizes)
        {
            if (upright)
            {
                size.transpose();
            }
            if (size.width() >= panel.width() || size.height() >= panel.height() || !same_shape(size, panel))
            {
                continue;
            }
            for (const ScreenMode& own : reported)
            {
                if (!has(size.width(), size.height(), own.refresh))
                {
                    modes.append({size.width(), size.height(), own.refresh, false});
                }
            }
        }
    }

    // A mode chosen elsewhere, or one of these from an earlier session.
    if (current.width > 0 && !reported.isEmpty() && !has(current.width, current.height, current.refresh))
    {
        modes.append({current.width, current.height, current.refresh, false});
    }
    return modes;
}
