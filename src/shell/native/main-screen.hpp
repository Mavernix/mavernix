// Tells the compositor's "x11-settings" plugin which screen is the main one
// (the menu bar's and dock's), which it makes X11's primary screen: games
// under Xwayland open there and offer its resolutions. Set again whenever
// the main screen changes; the shell decides which one it is.
//
// Usage in QML:
//   MainScreen { name: shell.mainScreen ? shell.mainScreen.name : "" }

#pragma once

#include <QObject>
#include <QString>

#include "compositor-socket.hpp"

class MainScreen : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

  public:
    explicit MainScreen(QObject* parent = nullptr);

    QString name() const { return screenName; }
    void setName(const QString& name);

  signals:
    void nameChanged();

  private:
    void send();

    CompositorSocket compositor;
    QString screenName;
};
