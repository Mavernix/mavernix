#include "lid-watch.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

static const QString POWER_SERVICE = QStringLiteral("org.freedesktop.UPower");
static const QString POWER_PATH = QStringLiteral("/org/freedesktop/UPower");
static const QString POWER_INTERFACE = QStringLiteral("org.freedesktop.UPower");
static const QString PROPERTIES_INTERFACE = QStringLiteral("org.freedesktop.DBus.Properties");

static const QString LOGIN_SERVICE = QStringLiteral("org.freedesktop.login1");
static const QString LOGIN_PATH = QStringLiteral("/org/freedesktop/login1");
static const QString LOGIN_MANAGER = QStringLiteral("org.freedesktop.login1.Manager");

LidWatch::LidWatch(QObject* parent) : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
    {
        return;
    }
    bus.connect(POWER_SERVICE, POWER_PATH, PROPERTIES_INTERFACE, QStringLiteral("PropertiesChanged"), this,
        SLOT(powerChanged(QString, QVariantMap, QStringList)));
    readLid(false);
}

void LidWatch::setHandlesLid(bool handles)
{
    if (handles == handles_lid)
    {
        return;
    }
    handles_lid = handles;
    if (handles && present)
    {
        holdLid();
    }
    else
    {
        // Closing the descriptor gives the lid back to logind.
        lid_lock = QDBusUnixFileDescriptor();
    }
    emit handlesLidChanged();
}

void LidWatch::powerChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated)
{
    if (interface != POWER_INTERFACE)
    {
        return;
    }
    if (changed.contains(QStringLiteral("LidIsClosed")))
    {
        changeLid(changed.value(QStringLiteral("LidIsClosed")).toBool());
    }
    else if (invalidated.contains(QStringLiteral("LidIsClosed")))
    {
        readLid(true);
    }
}

void LidWatch::readLid(bool announce)
{
    QDBusMessage ask =
        QDBusMessage::createMethodCall(POWER_SERVICE, POWER_PATH, PROPERTIES_INTERFACE, QStringLiteral("GetAll"));
    ask << POWER_INTERFACE;
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(ask), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, announce](QDBusPendingCallWatcher* finished) {
        const QDBusPendingReply<QVariantMap> reply = *finished;
        finished->deleteLater();
        if (reply.isError())
        {
            return;
        }
        // Only a lid that is seen is taken from logind: one we could not
        // see close would then do nothing at all.
        present = reply.value().value(QStringLiteral("LidIsPresent")).toBool();
        if (present && handles_lid && !lid_lock.isValid())
        {
            holdLid();
        }
        const bool now_closed = reply.value().value(QStringLiteral("LidIsClosed")).toBool();
        if (announce)
        {
            changeLid(now_closed);
        }
        else
        {
            closed = now_closed;
        }
    });
}

void LidWatch::holdLid()
{
    QDBusMessage ask =
        QDBusMessage::createMethodCall(LOGIN_SERVICE, LOGIN_PATH, LOGIN_MANAGER, QStringLiteral("Inhibit"));
    ask << QStringLiteral("handle-lid-switch") << QStringLiteral("Desktop shell")
        << QStringLiteral("The desktop's own choice of what closing the lid does") << QStringLiteral("block");
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(ask), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* finished) {
        const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *finished;
        // Given back meanwhile: the answer comes too late to be kept.
        if (!reply.isError() && handles_lid)
        {
            lid_lock = reply.value();
        }
        finished->deleteLater();
    });
}

void LidWatch::changeLid(bool now_closed)
{
    // Any move drops an answer still to come, an opening before it too.
    const int move = ++lid_moves;
    if (!now_closed)
    {
        if (closed)
        {
            closed = false;
            emit closedChanged();
        }
        return;
    }
    if (closed)
    {
        return;
    }

    QDBusMessage ask =
        QDBusMessage::createMethodCall(LOGIN_SERVICE, LOGIN_PATH, PROPERTIES_INTERFACE, QStringLiteral("Get"));
    ask << LOGIN_MANAGER << QStringLiteral("Docked");
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(ask), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, move](QDBusPendingCallWatcher* finished) {
        const QDBusPendingReply<QVariant> reply = *finished;
        finished->deleteLater();
        // Opened again meanwhile.
        if (move != lid_moves)
        {
            return;
        }
        // Not known: not docked, as logind would have it without a dock.
        docked = !reply.isError() && reply.value().toBool();
        closed = true;
        emit closedChanged();
    });
}
