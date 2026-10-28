#include "bluetooth-agent.hpp"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QDebug>

namespace
{
const QString SERVICE = QStringLiteral("org.bluez");
const QString AGENT_PATH = QStringLiteral("/org/mavernix/bluetooth_agent");
// Can show a code and take one typed: every way of pairing works.
const QString CAPABILITY = QStringLiteral("KeyboardDisplay");
const QString REJECTED = QStringLiteral("org.bluez.Error.Rejected");
const QString CANCELED = QStringLiteral("org.bluez.Error.Canceled");
} // namespace

BluetoothAgent::BluetoothAgent(QObject* parent) : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.registerObject(AGENT_PATH, this, QDBusConnection::ExportScriptableSlots))
    {
        qWarning().noquote() << "Shell.Native: could not offer the Bluetooth pairing agent -" << bus.lastError().message();
        return;
    }
    auto* watcher = new QDBusServiceWatcher(SERVICE, bus, QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &BluetoothAgent::registerWithBlueZ);
    registerWithBlueZ();
}

BluetoothAgent::~BluetoothAgent()
{
    QDBusMessage leaving = QDBusMessage::createMethodCall(
        SERVICE, QStringLiteral("/org/bluez"), QStringLiteral("org.bluez.AgentManager1"), QStringLiteral("UnregisterAgent"));
    leaving << QVariant::fromValue(QDBusObjectPath(AGENT_PATH));
    QDBusConnection::systemBus().call(leaving, QDBus::NoBlock);
    QDBusConnection::systemBus().unregisterObject(AGENT_PATH);
}

void BluetoothAgent::registerWithBlueZ()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.interface()->isServiceRegistered(SERVICE))
    {
        return;
    }
    const auto manager = [](const char* method) {
        return QDBusMessage::createMethodCall(
            SERVICE, QStringLiteral("/org/bluez"), QStringLiteral("org.bluez.AgentManager1"), QLatin1String(method));
    };
    QDBusMessage registering = manager("RegisterAgent");
    registering << QVariant::fromValue(QDBusObjectPath(AGENT_PATH)) << CAPABILITY;
    bus.asyncCall(registering);
    // The agent for pairings no program asked for, such as from the menu.
    QDBusMessage defaulting = manager("RequestDefaultAgent");
    defaulting << QVariant::fromValue(QDBusObjectPath(AGENT_PATH));
    bus.asyncCall(defaulting);
}

QString BluetoothAgent::deviceName(const QDBusObjectPath& device) const
{
    QDBusMessage asking =
        QDBusMessage::createMethodCall(SERVICE, device.path(), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    asking << QStringLiteral("org.bluez.Device1") << QStringLiteral("Alias");
    const QDBusReply<QDBusVariant> answer = QDBusConnection::systemBus().call(asking, QDBus::Block, 1000);
    return answer.isValid() ? answer.value().variant().toString() : device.path().section(QLatin1Char('/'), -1);
}

void BluetoothAgent::wait(const QString& kind, const QDBusObjectPath& device, const QString& code)
{
    // A new question replaces an unanswered one.
    if (pending.type() == QDBusMessage::MethodCallMessage)
    {
        QDBusConnection::systemBus().send(pending.createErrorReply(CANCELED, QString()));
    }
    setDelayedReply(true);
    pending = message();
    pending_kind = kind;
    emit asked(kind, deviceName(device), code);
}

void BluetoothAgent::answer(bool accepted, const QString& typed)
{
    if (pending.type() != QDBusMessage::MethodCallMessage)
    {
        return;
    }
    QDBusMessage reply;
    if (!accepted)
    {
        reply = pending.createErrorReply(REJECTED, QString());
    }
    else if (pending_kind == QLatin1String("pin"))
    {
        reply = pending.createReply(typed);
    }
    else if (pending_kind == QLatin1String("passkey"))
    {
        reply = pending.createReply(QVariant::fromValue(typed.toUInt()));
    }
    else
    {
        reply = pending.createReply();
    }
    QDBusConnection::systemBus().send(reply);
    pending = QDBusMessage();
}

void BluetoothAgent::Release() {}

QString BluetoothAgent::RequestPinCode(const QDBusObjectPath& device)
{
    wait(QStringLiteral("pin"), device, QString());
    return QString();
}

void BluetoothAgent::DisplayPinCode(const QDBusObjectPath& device, const QString& pincode)
{
    emit asked(QStringLiteral("show"), deviceName(device), pincode);
}

uint BluetoothAgent::RequestPasskey(const QDBusObjectPath& device)
{
    wait(QStringLiteral("passkey"), device, QString());
    return 0;
}

void BluetoothAgent::DisplayPasskey(const QDBusObjectPath& device, uint passkey, ushort entered)
{
    // Shown once; the later calls only count the digits typed on the device.
    if (entered == 0)
    {
        emit asked(QStringLiteral("show"), deviceName(device), QStringLiteral("%1").arg(passkey, 6, 10, QLatin1Char('0')));
    }
}

void BluetoothAgent::RequestConfirmation(const QDBusObjectPath& device, uint passkey)
{
    wait(QStringLiteral("confirm"), device, QStringLiteral("%1").arg(passkey, 6, 10, QLatin1Char('0')));
}

void BluetoothAgent::RequestAuthorization(const QDBusObjectPath& device)
{
    wait(QStringLiteral("authorize"), device, QString());
}

void BluetoothAgent::AuthorizeService(const QDBusObjectPath& device, const QString& uuid)
{
    Q_UNUSED(device)
    Q_UNUSED(uuid)
    // A paired, trusted device may use its services; BlueZ only asks for
    // untrusted ones, which were paired by this person a moment ago.
}

void BluetoothAgent::Cancel()
{
    pending = QDBusMessage();
    emit cancelled();
}
