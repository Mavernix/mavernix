#include "panel-modes.hpp"

#include <QSize>

#include <cstdlib>

// Common sizes of the usual shapes (16:10, 16:9, 3:2, 4:3), the ones
// programs and games expect. Only those of the panel's own shape are
// offered: any other is drawn with black bars or out of proportion.
static const QSize common_sizes[] = {
    {3840, 2400},
    {3840, 2160},
    {3200, 2000},
    {3200, 1800},
    {3000, 2000},
    {2880, 1920},
    {2880, 1800},
    {2880, 1620},
    {2560, 1600},
    {2560, 1440},
    {2400, 1600},
    {2304, 1440},
    {2256, 1504},
    {2048, 1536},
    {2048, 1280},
    {2048, 1152},
    {1920, 1440},
    {1920, 1280},
    {1920, 1200},
    {1920, 1080},
    {1680, 1120},
    {1680, 1050},
    {1600, 1200},
    {1600, 900},
    {1440, 960},
    {1440, 900},
    {1400, 1050},
    {1280, 960},
    {1280, 800},
    {1280, 720},
    {1024, 768},
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
    return name.startsWith(QLatin1String("eDP-")) || name.startsWith(QLatin1String("LVDS-")) ||
           name.startsWith(QLatin1String("DSI-"));
}

// The common sizes smaller than `panel` and of its shape.
static QList<QSize> stretched_sizes(const QSize& panel)
{
    QList<QSize> sizes;
    // A panel built upright, as a tablet's, has the same sizes on end.
    const bool upright = panel.height() > panel.width();
    for (QSize size : common_sizes)
    {
        if (upright)
        {
            size.transpose();
        }
        if (size.width() < panel.width() && size.height() < panel.height() && same_shape(size, panel))
        {
            sizes.append(size);
        }
    }
    return sizes;
}

QList<ScreenMode> offered_modes(const QString& name, const ScreenMode& current, const QList<ScreenMode>& reported)
{
    QList<ScreenMode> modes = reported;
    // A custom mode's rate comes back from its timings a hair off the one asked.
    const auto has = [&modes](int width, int height, int refresh) {
        for (const ScreenMode& mode : modes)
        {
            if (mode.width == width && mode.height == height &&
                std::abs(mode.refresh - refresh) <= same_rate_millihertz)
            {
                return true;
            }
        }
        return false;
    };

    // The panel's own size: its preferred mode's, else the largest.
    QSize panel;
    for (const ScreenMode& mode : reported)
    {
        const QSize size(mode.width, mode.height);
        if (mode.preferred || qint64(size.width()) * size.height() > qint64(panel.width()) * panel.height())
        {
            panel = size;
            if (mode.preferred)
            {
                break;
            }
        }
    }
    const QList<QSize> stretched = stretched_sizes(panel);

    // A screen that names more sizes than its own has its driver's list. One
    // of these, chosen before, comes back as a size it names: still its own.
    bool one_size = !reported.isEmpty() && is_built_in(name);
    for (const ScreenMode& mode : reported)
    {
        const QSize size(mode.width, mode.height);
        one_size = one_size && (size == panel || stretched.contains(size));
    }
    if (one_size)
    {
        for (const QSize& size : stretched)
        {
            for (const ScreenMode& own : reported)
            {
                if (QSize(own.width, own.height) == panel && !has(size.width(), size.height(), own.refresh))
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
