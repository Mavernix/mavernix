// Slides a screen's desktop (its picture, icons and windows) sideways, and
// back, through the compositor's "desktop-aside" plugin; the menu bar, the
// dock and whatever is uncovered stay put. The plugin brings the desktop
// home by itself if the shell goes away.
//
// Usage in QML:
//   DesktopAside { id: aside }
//   aside.move(screen.name, -300, 265, "ease-in-out")   (aside, to the left)
//   aside.move(screen.name, 0, 265, "ease-in-out")      (home)

#pragma once

#include <QObject>

#include "compositor-socket.hpp"

class DesktopAside : public QObject
{
    Q_OBJECT

  public:
    explicit DesktopAside(QObject* parent = nullptr);

    // `by` in points, negative to the left; `curve` is one of the theme's
    // curve names.
    Q_INVOKABLE void move(const QString& screenName, int by, int durationMs, const QString& curve);

  private:
    CompositorSocket compositor;
};
