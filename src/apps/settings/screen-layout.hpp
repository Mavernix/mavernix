// Where a screen let go in the Display pane's picture lands: beside another,
// in the desktop's own (logical) pixels. Tested in
// tests/test-screen-layout.cpp; the session does the same when a change of
// size leaves screens apart (make_wayfire_config.py).

#pragma once

#include <QList>
#include <QMap>
#include <QPoint>
#include <QRect>
#include <QString>

// Two screens touch when they share at least this much of an edge (or all of
// the shorter one's); less, and the pointer could hardly cross.
inline constexpr int least_shared_edge_px = 100;

// The place nearest to where `moved` is that touches one of `others` and
// covers none of them. A place within `line_up` of lining up with an edge
// lines up with it.
QPoint snapped(const QRect& moved, const QList<QRect>& others, int line_up);

// Whether `one` and `other` share enough of an edge (least_shared_edge_px).
bool touching(const QRect& one, const QRect& other);

// Each screen's top left corner, moved so the main screen is at 0,0, as the
// compositor's config writes it.
QMap<QString, QPoint> positions(const QMap<QString, QRect>& screens, const QString& main);
