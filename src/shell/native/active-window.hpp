// The window that has the keyboard: its application and where its menu can
// be fetched. Wayland applications announce their menu's bus address through
// the "kde-appmenu" protocol; Wayfire exposes it as two window properties.
// X11 applications register it with the shell's menu registrar
// (menu-registrar.hpp), found here by the window's X11 ID, else its process.
//
// Usage in QML:
//   ActiveWindow { id: activeWindow }
//   ApplicationMenu { service: activeWindow.menuService; path: activeWindow.menuPath }

#pragma once

#include <QObject>
#include <QTimer>

#include "compositor-socket.hpp"

class ActiveWindow : public QObject
{
    Q_OBJECT
    // Empty if no window is active.
    Q_PROPERTY(QString appId READ appId NOTIFY changed)
    // Both empty if the window has no menu.
    Q_PROPERTY(QString menuService READ menuService NOTIFY changed)
    Q_PROPERTY(QString menuPath READ menuPath NOTIFY changed)

  public:
    explicit ActiveWindow(QObject* parent = nullptr);

    QString appId() const;
    QString menuService() const;
    QString menuPath() const;

  signals:
    void changed();

  private:
    void startWatching();
    void takeWindow(const QJsonValue& window);
    void askForMenuAddress();
    void askForX11Menu(int askedFor);
    void setMenuAddress(const QString& service, const QString& path);

    CompositorSocket compositor;

    // -1 for none.
    int windowId = -1;
    // The window's process; for an X11 window, the one that drew it.
    uint windowPid = 0;
    QString currentAppId;
    QString currentMenuService;
    QString currentMenuPath;

    // The menu is announced a moment after the window appears, with no
    // event for it, so the address is polled a few times.
    QTimer askAgain;
    int askAgainCount = 0;
};
