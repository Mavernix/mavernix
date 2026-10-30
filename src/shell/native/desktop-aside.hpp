// Slides a screen's desktop (its picture, icons and windows) sideways, and
// back, through the compositor's "desktop-aside" plugin; the menu bar, the
// dock and whatever is uncovered stay put. The plugin brings the desktop
// home by itself if the shell goes away. A press on the desktop while it is
// aside reaches no window; `pressed` tells of it instead. The connection to
// the compositor is opened by the first move, so a shell that never moves
// the desktop has none.
//
// Usage in QML:
//   DesktopAside { id: aside; onPressed: screenName => ... }
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

  signals:
    void pressed(const QString& screenName);

  private:
    void send(const QJsonObject& question);

    CompositorSocket compositor;
    bool opened = false;
    // The latest move asked for while the connection was being made.
    QJsonObject waitingMove;
};
