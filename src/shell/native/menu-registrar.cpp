#include "menu-registrar.hpp"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusServiceWatcher>
#include <QDebug>

namespace
{
const QString REGISTRAR_NAME = QStringLiteral("com.canonical.AppMenu.Registrar");
const QString REGISTRAR_PATH = QStringLiteral("/com/canonical/AppMenu/Registrar");
} // namespace

X11Menus& X11Menus::instance()
{
    static X11Menus menus;
    return menus;
}

X11Menu X11Menus::forProcess(uint pid) const
{
    const Entry* latest = nullptr;
    for (const Entry& entry : windows)
    {
        if (entry.pid == pid && (latest == nullptr || entry.order > latest->order))
        {
            latest = &entry;
        }
    }
    return latest != nullptr ? latest->menu : X11Menu{};
}

X11Menu X11Menus::forWindow(uint window) const
{
    return windows.value(window).menu;
}

void X11Menus::add(uint window, uint pid, const X11Menu& menu)
{
    windows.insert(window, {pid, menu, ++registrations});
    emit changed();
}

void X11Menus::remove(uint window)
{
    if (windows.remove(window) > 0)
    {
        emit changed();
    }
}

void X11Menus::removeService(const QString& service)
{
    bool removed = false;
    for (auto entry = windows.begin(); entry != windows.end();)
    {
        if (entry->menu.service == service)
        {
            entry = windows.erase(entry);
            removed = true;
        }
        else
        {
            ++entry;
        }
    }
    if (removed)
    {
        emit changed();
    }
}

RegistrarObject::RegistrarObject(QObject* parent)
    : QObject(parent), leaving(QString(), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration)
{
    connect(&leaving, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString& service) {
        X11Menus::instance().removeService(service);
        leaving.removeWatchedService(service);
    });
}

void RegistrarObject::RegisterWindow(uint windowId, const QDBusObjectPath& menuObjectPath)
{
    const QString service = message().service();
    const uint pid = connection().interface()->servicePid(service).value();
    X11Menus::instance().add(windowId, pid, {service, menuObjectPath.path()});
    leaving.addWatchedService(service);
    emit WindowRegistered(windowId, service, menuObjectPath);
}

void RegistrarObject::UnregisterWindow(uint windowId)
{
    X11Menus::instance().remove(windowId);
    emit WindowUnregistered(windowId);
}

QString RegistrarObject::GetMenuForWindow(uint windowId, QDBusObjectPath& menuObjectPath)
{
    const X11Menu menu = X11Menus::instance().forWindow(windowId);
    menuObjectPath = QDBusObjectPath(menu.path.isEmpty() ? QStringLiteral("/") : menu.path);
    return menu.service;
}

MenuRegistrar::MenuRegistrar(QObject* parent) : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    // The object first: an application may call it as soon as the name is ours.
    bus.registerObject(REGISTRAR_PATH, &object, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    registered = bus.registerService(REGISTRAR_NAME);
    if (!registered)
    {
        bus.unregisterObject(REGISTRAR_PATH);
        qWarning().noquote() << "Shell.Native: could not take the bus name" << REGISTRAR_NAME << "-"
                             << bus.lastError().message()
                             << "(applications started now will keep their menus in their windows;"
                             << "check that no other menu bar is running)";
    }
}

MenuRegistrar::~MenuRegistrar()
{
    if (registered)
    {
        QDBusConnection::sessionBus().unregisterService(REGISTRAR_NAME);
        QDBusConnection::sessionBus().unregisterObject(REGISTRAR_PATH);
    }
}

bool MenuRegistrar::isRegistered() const
{
    return registered;
}
