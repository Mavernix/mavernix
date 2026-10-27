#include "session-watch.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

static const QString LOGIN_SERVICE = QStringLiteral("org.freedesktop.login1");
static const QString LOGIN_PATH = QStringLiteral("/org/freedesktop/login1");
static const QString LOGIN_MANAGER = QStringLiteral("org.freedesktop.login1.Manager");
static const QString LOGIN_SESSION = QStringLiteral("org.freedesktop.login1.Session");

// logind itself gives up after five seconds.
static const int LONGEST_WAIT_MS = 2000;

SessionWatch::SessionWatch(QObject* parent) : QObject(parent)
{
    longestWait.setSingleShot(true);
    longestWait.setInterval(LONGEST_WAIT_MS);
    connect(&longestWait, &QTimer::timeout, this, &SessionWatch::readyToSleep);

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
    {
        return;
    }

    bus.connect(
        LOGIN_SERVICE, LOGIN_PATH, LOGIN_MANAGER, QStringLiteral("PrepareForSleep"), this, SLOT(sleepIsNear(bool)));

    // Lock and Unlock are signals of this process's session object.
    QDBusMessage ask =
        QDBusMessage::createMethodCall(LOGIN_SERVICE, LOGIN_PATH, LOGIN_MANAGER, QStringLiteral("GetSessionByPID"));
    ask << static_cast<uint>(QCoreApplication::applicationPid());
    auto* watcher = new QDBusPendingCallWatcher(bus.asyncCall(ask), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* finished) {
        const QDBusPendingReply<QDBusObjectPath> reply = *finished;
        if (!reply.isError())
        {
            sessionFound(reply.value());
        }
        finished->deleteLater();
    });

    holdSleepBack();
}

void SessionWatch::sessionFound(const QDBusObjectPath& path)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(LOGIN_SERVICE, path.path(), LOGIN_SESSION, QStringLiteral("Lock"), this, SIGNAL(lockRequested()));
    bus.connect(LOGIN_SERVICE, path.path(), LOGIN_SESSION, QStringLiteral("Unlock"), this, SIGNAL(unlockRequested()));
}

void SessionWatch::holdSleepBack()
{
    QDBusMessage ask =
        QDBusMessage::createMethodCall(LOGIN_SERVICE, LOGIN_PATH, LOGIN_MANAGER, QStringLiteral("Inhibit"));
    ask << QStringLiteral("sleep") << QStringLiteral("Desktop shell")
        << QStringLiteral("Locking the screen before sleep") << QStringLiteral("delay");
    auto* watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(ask), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* finished) {
        const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *finished;
        if (!reply.isError())
        {
            delay = reply.value();
        }
        finished->deleteLater();
    });
}

void SessionWatch::sleepIsNear(bool going_to_sleep)
{
    if (going_to_sleep)
    {
        longestWait.start();
        emit aboutToSleep();
    }
    else
    {
        holdSleepBack();
        emit wokeUp();
    }
}

void SessionWatch::readyToSleep()
{
    longestWait.stop();
    // Closing the descriptor releases the inhibitor.
    delay = QDBusUnixFileDescriptor();
}

void SessionWatch::sleep()
{
    QDBusMessage ask =
        QDBusMessage::createMethodCall(LOGIN_SERVICE, LOGIN_PATH, LOGIN_MANAGER, QStringLiteral("Suspend"));
    // Not interactive: an idle machine has nobody to ask for a password.
    ask << false;
    QDBusConnection::systemBus().asyncCall(ask);
}
