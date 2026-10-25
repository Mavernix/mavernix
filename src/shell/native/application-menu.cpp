#include "application-menu.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDateTime>

namespace
{
const QString MENU_INTERFACE = QStringLiteral("com.canonical.dbusmenu");

const int ROOT_ITEM = 0;
const int WHOLE_TREE = -1;

// Strips mnemonic marks ("_File"); "__" stands for a real underscore.
QString withoutMnemonic(const QString& label)
{
    QString result;
    for (qsizetype position = 0; position < label.size(); position++)
    {
        const bool isUnderscore = label[position] == '_';
        const bool nextIsUnderscore = (position + 1 < label.size()) && (label[position + 1] == '_');
        if (isUnderscore && nextIsUnderscore)
        {
            result += '_';
            position++;
        }
        else if (!isUnderscore)
        {
            result += label[position];
        }
    }
    return result;
}

// The "shortcut" property is a list of combinations: [["Control", "S"]].
QStringList firstShortcut(const QVariant& property)
{
    if (!property.canConvert<QDBusArgument>())
    {
        return {};
    }

    QList<QStringList> combinations;
    property.value<QDBusArgument>() >> combinations;
    return combinations.isEmpty() ? QStringList() : combinations.first();
}

// Reads an item and its subtree. D-Bus signature (ia{sv}av): id,
// properties, and children wrapped in variants.
QVariantMap readItem(const QDBusArgument& argument)
{
    int id = 0;
    QVariantMap properties;
    QVariantList children;

    argument.beginStructure();
    argument >> id >> properties;
    argument.beginArray();
    while (!argument.atEnd())
    {
        QDBusVariant wrappedChild;
        argument >> wrappedChild;
        const QVariantMap child = readItem(wrappedChild.variant().value<QDBusArgument>());
        if (child.value("visible", true).toBool())
        {
            children.append(child);
        }
    }
    argument.endArray();
    argument.endStructure();

    const QString toggleType = properties.value("toggle-type").toString();
    return {
        {"id", id},
        {"label", withoutMnemonic(properties.value("label").toString())},
        {"enabled", properties.value("enabled", true).toBool()},
        {"visible", properties.value("visible", true).toBool()},
        {"separator", properties.value("type").toString() == "separator"},
        {"checkable", !toggleType.isEmpty()},
        {"checked", properties.value("toggle-state", 0).toInt() == 1},
        {"shortcut", firstShortcut(properties.value("shortcut"))},
        {"children", children},
    };
}
} // namespace

ApplicationMenu::ApplicationMenu(QObject* parent) : QObject(parent)
{
    fetchTimer.setSingleShot(true);
    fetchTimer.setInterval(0);
    connect(&fetchTimer, &QTimer::timeout, this, &ApplicationMenu::fetch);
}

QString ApplicationMenu::service() const
{
    return currentService;
}

void ApplicationMenu::setService(const QString& service)
{
    if (service == currentService)
    {
        return;
    }
    currentService = service;
    followAddress();
}

QString ApplicationMenu::path() const
{
    return currentPath;
}

void ApplicationMenu::setPath(const QString& path)
{
    if (path == currentPath)
    {
        return;
    }
    currentPath = path;
    followAddress();
}

QVariantList ApplicationMenu::items() const
{
    return currentItems;
}

void ApplicationMenu::followAddress()
{
    addressGeneration++;
    emit addressChanged();

    // Empty service and path remove every match this object made.
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(QString(), QString(), MENU_INTERFACE, "LayoutUpdated", this, SLOT(fetchSoon()));
    bus.disconnect(QString(), QString(), MENU_INTERFACE, "ItemsPropertiesUpdated", this, SLOT(fetchSoon()));

    setItems({});
    if (currentService.isEmpty() || currentPath.isEmpty())
    {
        return;
    }

    bus.connect(currentService, currentPath, MENU_INTERFACE, "LayoutUpdated", this, SLOT(fetchSoon()));
    bus.connect(currentService, currentPath, MENU_INTERFACE, "ItemsPropertiesUpdated", this, SLOT(fetchSoon()));
    fetchSoon();
}

void ApplicationMenu::fetchSoon()
{
    fetchTimer.start();
}

void ApplicationMenu::fetch()
{
    if (currentService.isEmpty() || currentPath.isEmpty())
    {
        return;
    }

    // GetLayout(parent, depth, property names); no names means all.
    QDBusMessage call = QDBusMessage::createMethodCall(currentService, currentPath, MENU_INTERFACE, "GetLayout");
    call << ROOT_ITEM << WHOLE_TREE << QStringList();

    const int askedInGeneration = addressGeneration;
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, askedInGeneration](QDBusPendingCallWatcher* finished) {
        finished->deleteLater();
        if (askedInGeneration != addressGeneration)
        {
            return;
        }

        const QDBusMessage answer = finished->reply();
        if (answer.type() != QDBusMessage::ReplyMessage || answer.arguments().size() < 2)
        {
            // An application that has just quit is not worth a warning.
            const bool applicationGone =
                answer.errorName() == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown");
            if (!applicationGone)
            {
                qWarning().noquote() << "Shell.Native: could not fetch the menu at" << currentService << currentPath
                                     << "-" << answer.errorMessage();
            }
            setItems({});
            return;
        }

        // The answer is a revision number and the root item.
        const QVariantMap root = readItem(answer.arguments().at(1).value<QDBusArgument>());
        setItems(root.value("children").toList());
    });
}

void ApplicationMenu::setItems(const QVariantList& newItems)
{
    if (newItems == currentItems)
    {
        return;
    }
    currentItems = newItems;
    emit itemsChanged();
}

void ApplicationMenu::aboutToShow(int id)
{
    if (currentService.isEmpty())
    {
        return;
    }

    // Any change this causes arrives as LayoutUpdated, so the answer is ignored.
    QDBusMessage call = QDBusMessage::createMethodCall(currentService, currentPath, MENU_INTERFACE, "AboutToShow");
    call << id;
    QDBusConnection::sessionBus().asyncCall(call);
}

void ApplicationMenu::activate(int id)
{
    if (currentService.isEmpty())
    {
        return;
    }

    // Event(item, name, data, time); "clicked" still needs a data value.
    const quint32 timestamp = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
    QDBusMessage call = QDBusMessage::createMethodCall(currentService, currentPath, MENU_INTERFACE, "Event");
    call << id << QStringLiteral("clicked") << QVariant::fromValue(QDBusVariant(QString())) << timestamp;
    QDBusConnection::sessionBus().asyncCall(call);
}
