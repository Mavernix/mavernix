// The compositor's desktops (Wayfire workspaces in one row): how many, which
// is shown, and switching. The count is a session setting; this class only
// moves windows out of the way before it shrinks. Only the first screen is
// followed.
//
// Usage in QML:
//   Desktops { id: desktops }
//   desktops.show(1)        (the first desktop is 0)
//   desktops.closeGap(1)    then, on `gapClosed`, lower the setting by one

#pragma once

#include <QObject>

#include "compositor-socket.hpp"

class Desktops : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int current READ current NOTIFY changed)
    // In points; also the offset from one desktop to the next.
    Q_PROPERTY(qreal screenWidth READ screenWidth NOTIFY changed)

  public:
    explicit Desktops(QObject* parent = nullptr);

    int count() const;
    int current() const;
    qreal screenWidth() const;

    Q_INVOKABLE void show(int index);
    // The compositor sends no event when the count changes; whoever
    // changed the setting calls this.
    Q_INVOKABLE void refresh();
    // Before a desktop is removed: shifts the windows of it and every later
    // desktop one to the left so that the last one is empty, then emits
    // `gapClosed`.
    Q_INVOKABLE void closeGap(int index);

    // Coordinates are relative to the desktop shown.
    Q_INVOKABLE void placeWindow(int id, qreal x, qreal y, qreal windowWidth, qreal windowHeight);

  signals:
    void changed();
    void gapClosed();

  private:
    void startWatching();
    void takeScreen(const QJsonObject& screen);

    CompositorSocket compositor;

    // -1 until the compositor has answered.
    int screenId = -1;
    int desktopCount = 1;
    int currentDesktop = 0;
    qreal width = 0;
};
