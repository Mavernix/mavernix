#include "menu-registrar.hpp"

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

namespace
{
const QString REGISTRAR_NAME = QStringLiteral("com.canonical.AppMenu.Registrar");
}

MenuRegistrar::MenuRegistrar(QObject* parent) : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    registered = bus.registerService(REGISTRAR_NAME);
    if (!registered)
    {
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
    }
}

bool MenuRegistrar::isRegistered() const
{
    return registered;
}
