// Owns com.canonical.AppMenu.Registrar, which makes applications export
// their menu bar. On Wayland the address goes to the compositor ("kde-appmenu"),
// so owning the name is all that is needed. X11 applications (Qt on xcb,
// Electron, Chromium, LibreOffice) instead call RegisterWindow with their X11
// window's ID; those menus are kept in X11Menus, for ActiveWindow.

#pragma once

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QObject>
#include <QString>

// Where an X11 application's menu can be fetched.
struct X11Menu
{
    QString service;
    QString path;
};

// The menus X11 applications registered, for the life of the shell.
class X11Menus : public QObject
{
    Q_OBJECT

  public:
    static X11Menus& instance();

    // Wayfire tells a window's process, not its X11 ID: the menu the
    // process registered last. Empty if none.
    X11Menu forProcess(uint pid) const;
    X11Menu forWindow(uint window) const;

    void add(uint window, uint pid, const X11Menu& menu);
    void remove(uint window);
    // The application left the bus.
    void removeService(const QString& service);

  signals:
    void changed();

  private:
    struct Entry
    {
        uint pid = 0;
        X11Menu menu;
        // Later registrations win.
        quint64 order = 0;
    };
    QHash<uint, Entry> windows;
    quint64 registrations = 0;
};

// The bus object at /com/canonical/AppMenu/Registrar.
class RegistrarObject : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.AppMenu.Registrar")

  public:
    explicit RegistrarObject(QObject* parent = nullptr);

  public slots:
    void RegisterWindow(uint windowId, const QDBusObjectPath& menuObjectPath);
    void UnregisterWindow(uint windowId);
    QString GetMenuForWindow(uint windowId, QDBusObjectPath& menuObjectPath);

  signals:
    void WindowRegistered(uint windowId, const QString& service, const QDBusObjectPath& menuObjectPath);
    void WindowUnregistered(uint windowId);

  private:
    // Forgets an application's menus once it leaves the bus.
    QDBusServiceWatcher leaving;
};

class MenuRegistrar : public QObject
{
    Q_OBJECT
    // False if another program already owns the name, or there is no bus.
    Q_PROPERTY(bool registered READ isRegistered CONSTANT)

  public:
    explicit MenuRegistrar(QObject* parent = nullptr);
    ~MenuRegistrar() override;

    bool isRegistered() const;

  private:
    bool registered = false;
    RegistrarObject object;
};
