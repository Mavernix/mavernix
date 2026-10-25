// The window that has the keyboard: its application and where its menu can
// be fetched. Applications announce their menu's bus address through the
// "kde-appmenu" protocol; Wayfire exposes it as two window properties.
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
    void setMenuAddress(const QString& service, const QString& path);

    CompositorSocket compositor;

    // -1 for none.
    int windowId = -1;
    QString currentAppId;
    QString currentMenuService;
    QString currentMenuPath;

    // The menu is announced a moment after the window appears, with no
    // event for it, so the address is polled a few times.
    QTimer askAgain;
    int askAgainCount = 0;
};
