#include "bluetooth-pane.hpp"

#include <QCheckBox>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "choices.hpp"

// GetManagedObjects' answer: object path -> interface -> properties.
using InterfacesOfObject = QMap<QString, QVariantMap>;
using ObjectsOfService = QMap<QDBusObjectPath, InterfacesOfObject>;
Q_DECLARE_METATYPE(InterfacesOfObject)
Q_DECLARE_METATYPE(ObjectsOfService)

static const QString service_name = QStringLiteral("org.bluez");
static const char* adapter_interface = "org.bluez.Adapter1";
static const char* device_interface = "org.bluez.Device1";
static const char* properties_interface = "org.freedesktop.DBus.Properties";
static const char* objects_interface = "org.freedesktop.DBus.ObjectManager";

// Changes come in bursts (a device connecting changes several things);
// the list is read once after this long.
static const int reading_delay_ms = 150;
// Some six devices without scrolling.
static const QSize list_size(380, 190);
static const int device_icon_size = 32;

BluetoothPane::BluetoothPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    qDBusRegisterMetaType<InterfacesOfObject>();
    qDBusRegisterMetaType<ObjectsOfService>();
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    powered = new QCheckBox(word("bluetooth_on", "Bluetooth on"));
    connect(powered, &QCheckBox::toggled, this, [this](bool on) {
        QDBusMessage change = QDBusMessage::createMethodCall(
            service_name, adapter_path, QLatin1String(properties_interface), QStringLiteral("Set"));
        change << QLatin1String(adapter_interface) << QStringLiteral("Powered")
               << QVariant::fromValue(QDBusVariant(on));
        QDBusConnection::systemBus().asyncCall(change);
    });

    devices = new QListWidget;
    devices->setIconSize(QSize(device_icon_size, device_icon_size));
    devices->setFixedSize(list_size);
    connect(devices, &QListWidget::currentRowChanged, this, &BluetoothPane::showChosen);

    connect_button = new QPushButton;
    forget_button = new QPushButton(word("bluetooth_forget", "Forget"));
    connect(connect_button, &QPushButton::clicked, this, [this]() {
        const QString path = devices->currentItem()->data(Qt::UserRole).toString();
        const QVariantMap device = known.value(path);
        if (!device.value(QStringLiteral("Paired")).toBool())
        {
            // Trusted and connected once paired (readAll), as from the menu.
            pairing_path = path;
            call(path, device_interface, "Pair");
        }
        else if (device.value(QStringLiteral("Connected")).toBool())
        {
            call(path, device_interface, "Disconnect");
        }
        else
        {
            call(path, device_interface, "Connect");
        }
    });
    connect(forget_button, &QPushButton::clicked, this, [this]() {
        const QString path = devices->currentItem()->data(Qt::UserRole).toString();
        const QString name = known.value(path).value(QStringLiteral("Alias")).toString();
        const auto sure = QMessageBox::question(this, word("bluetooth_forget", "Forget"),
            word("bluetooth_forget_question", "Forget “%1”? To use it again, it has to be paired anew.").arg(name),
            QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
        if (sure != QMessageBox::Yes)
        {
            return;
        }
        QDBusMessage forgetting = QDBusMessage::createMethodCall(
            service_name, adapter_path, QLatin1String(adapter_interface), QStringLiteral("RemoveDevice"));
        forgetting << QVariant::fromValue(QDBusObjectPath(path));
        QDBusConnection::systemBus().asyncCall(forgetting);
    });

    state_line = new QLabel;
    state_line->setEnabled(false);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(state_line);
    buttons->addStretch();
    buttons->addWidget(forget_button);
    buttons->addWidget(connect_button);

    auto* rows = new QVBoxLayout(this);
    rows->addWidget(powered);
    rows->addWidget(devices);
    rows->addLayout(buttons);

    reading_soon = new QTimer(this);
    reading_soon->setSingleShot(true);
    reading_soon->setInterval(reading_delay_ms);
    connect(reading_soon, &QTimer::timeout, this, &BluetoothPane::readAll);

    // Any property change or device coming or going (an empty path means any).
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(service_name, QString(), QLatin1String(properties_interface), QStringLiteral("PropertiesChanged"), this,
        SLOT(somethingChanged()));
    bus.connect(service_name, QString(), QLatin1String(objects_interface), QStringLiteral("InterfacesAdded"), this,
        SLOT(somethingChanged()));
    bus.connect(service_name, QString(), QLatin1String(objects_interface), QStringLiteral("InterfacesRemoved"), this,
        SLOT(somethingChanged()));

    readAll();
}

QString BluetoothPane::word(const char* key, const char* fallback) const
{
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}

void BluetoothPane::somethingChanged()
{
    reading_soon->start();
}

void BluetoothPane::call(const QString& path, const char* interface, const char* method)
{
    QDBusConnection::systemBus().asyncCall(
        QDBusMessage::createMethodCall(service_name, path, QLatin1String(interface), QLatin1String(method)));
}

void BluetoothPane::setSearching(bool searching)
{
    if (!adapter_path.isEmpty())
    {
        call(adapter_path, adapter_interface, searching ? "StartDiscovery" : "StopDiscovery");
    }
}

void BluetoothPane::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    setSearching(true);
}

void BluetoothPane::hideEvent(QHideEvent* event)
{
    setSearching(false);
    QWidget::hideEvent(event);
}

void BluetoothPane::readAll()
{
    QDBusMessage asking = QDBusMessage::createMethodCall(
        service_name, QStringLiteral("/"), QLatin1String(objects_interface), QStringLiteral("GetManagedObjects"));
    auto* waiting = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(asking), this);
    connect(waiting, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* finished) {
        finished->deleteLater();
        const QDBusPendingReply<ObjectsOfService> answer = *finished;
        const bool had_adapter = !adapter_path.isEmpty();
        adapter_path.clear();
        known.clear();
        bool radio_on = false;
        if (!answer.isError())
        {
            const ObjectsOfService objects = answer.value();
            for (auto object = objects.begin(); object != objects.end(); ++object)
            {
                if (object.value().contains(QLatin1String(adapter_interface)) && adapter_path.isEmpty())
                {
                    adapter_path = object.key().path();
                    radio_on = object.value()
                                   .value(QLatin1String(adapter_interface))
                                   .value(QStringLiteral("Powered"))
                                   .toBool();
                }
                if (object.value().contains(QLatin1String(device_interface)))
                {
                    known.insert(object.key().path(), object.value().value(QLatin1String(device_interface)));
                }
            }
        }

        {
            const QSignalBlocker quiet(powered);
            powered->setChecked(radio_on);
        }
        // A device paired from here is trusted, so it may reconnect by itself, and connected.
        const QVariantMap paired_now = known.value(pairing_path);
        if (!pairing_path.isEmpty() && paired_now.value(QStringLiteral("Paired")).toBool())
        {
            QDBusMessage trusting = QDBusMessage::createMethodCall(
                service_name, pairing_path, QLatin1String(properties_interface), QStringLiteral("Set"));
            trusting << QLatin1String(device_interface) << QStringLiteral("Trusted")
                     << QVariant::fromValue(QDBusVariant(true));
            QDBusConnection::systemBus().asyncCall(trusting);
            call(pairing_path, device_interface, "Connect");
            pairing_path.clear();
        }
        powered->setEnabled(!adapter_path.isEmpty());
        // The pane was on screen before the radio was known: look now.
        if (!had_adapter && isVisible())
        {
            setSearching(true);
        }

        // Known devices first; of those only found nearby, the ones with a name.
        const QString chosen_before =
            devices->currentItem() ? devices->currentItem()->data(Qt::UserRole).toString() : QString();
        const QSignalBlocker quiet(devices);
        devices->clear();
        for (const bool paired_ones : {true, false})
        {
            for (auto device = known.begin(); device != known.end(); ++device)
            {
                const QVariantMap& facts = device.value();
                const bool paired = facts.value(QStringLiteral("Paired")).toBool();
                if (paired != paired_ones || (!paired && !facts.contains(QStringLiteral("Name"))))
                {
                    continue;
                }
                QString state = word("bluetooth_not_paired", "Not paired");
                if (paired)
                {
                    state = facts.value(QStringLiteral("Connected")).toBool()
                                ? word("bluetooth_connected", "Connected")
                                : word("bluetooth_not_connected", "Not connected");
                }
                // The service names the kind of device as an icon's name.
                const QIcon icon = QIcon::fromTheme(
                    facts.value(QStringLiteral("Icon")).toString(), QIcon::fromTheme(QStringLiteral("bluetooth")));
                auto* row = new QListWidgetItem(
                    icon, facts.value(QStringLiteral("Alias")).toString() + QStringLiteral("\n") + state, devices);
                row->setData(Qt::UserRole, device.key());
                if (device.key() == chosen_before)
                {
                    devices->setCurrentItem(row);
                }
            }
        }
        devices->setEnabled(radio_on);

        if (adapter_path.isEmpty())
        {
            state_line->setText(word("bluetooth_none", "This computer has no Bluetooth."));
        }
        else
        {
            state_line->setText(radio_on ? word("bluetooth_searching", "Looking for devices…") : QString());
        }
        showChosen();
    });
}

void BluetoothPane::showChosen()
{
    const QListWidgetItem* chosen = devices->currentItem();
    const QVariantMap device = chosen ? known.value(chosen->data(Qt::UserRole).toString()) : QVariantMap();
    const bool paired = device.value(QStringLiteral("Paired")).toBool();

    connect_button->setEnabled(chosen != nullptr && devices->isEnabled());
    forget_button->setEnabled(chosen != nullptr && paired && devices->isEnabled());
    if (!paired && chosen != nullptr)
    {
        connect_button->setText(word("bluetooth_pair", "Pair"));
    }
    else if (device.value(QStringLiteral("Connected")).toBool())
    {
        connect_button->setText(word("bluetooth_disconnect", "Disconnect"));
    }
    else
    {
        connect_button->setText(word("bluetooth_connect", "Connect"));
    }
}
