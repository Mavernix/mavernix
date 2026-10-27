// The modes the Display pane offers for a screen: those it reports, and for
// a screen built into the computer that reports only its own size, the usual
// smaller sizes of the same shape. The graphics card stretches those to the
// panel, as other desktops do; Wayfire asks for them as custom modes. Plain
// work, tested in tests/test-panel-modes.cpp.

#pragma once

#include <QList>
#include <QString>

struct ScreenMode
{
    int width = 0;
    int height = 0;
    // In millihertz, as the compositor counts.
    int refresh = 0;
    bool preferred = false;
};

// Rates closer than this are one rate: a custom mode's comes back from its
// timings a hair off the one asked.
inline constexpr int same_rate_millihertz = 500;

// "eDP-1", "LVDS-1", "DSI-1": a laptop's or tablet's own panel.
bool is_built_in(const QString& name);

// What to offer for the screen `name` showing `current`: `reported`, and
// the stretched sizes where they make sense, each at the rates the panel's
// own size has. The current mode is always among them.
QList<ScreenMode> offered_modes(const QString& name, const ScreenMode& current, const QList<ScreenMode>& reported);
