#include "network-pane.hpp"

#include <QComboBox>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QIcon>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "choices.hpp"
#include "pane-parts.hpp"

// A connection's settings: setting name -> its properties.
using ConnectionSettings = QMap<QString, QVariantMap>;
Q_DECLARE_METATYPE(ConnectionSettings)

static const QString service_name = QStringLiteral("org.freedesktop.NetworkManager");
static const QString manager_path = QStringLiteral("/org/freedesktop/NetworkManager");
static const QString settings_path = QStringLiteral("/org/freedesktop/NetworkManager/Settings");
static const QString manager_interface = QStringLiteral("org.freedesktop.NetworkManager");
static const QString device_interface = QStringLiteral("org.freedesktop.NetworkManager.Device");
static const QString wireless_interface = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
static const QString point_interface = QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");
static const QString ip4_interface = QStringLiteral("org.freedesktop.NetworkManager.IP4Config");
static const QString settings_interface = QStringLiteral("org.freedesktop.NetworkManager.Settings");
static const QString connection_interface = QStringLiteral("org.freedesktop.NetworkManager.Settings.Connection");
static const QString properties_interface = QStringLiteral("org.freedesktop.DBus.Properties");

// NetworkManager's numbers (NMDeviceType, NMDeviceState, NM80211ApFlags,
// NM80211ApSecurityFlags).
static const uint ethernet_type = 1;
static const uint wifi_type = 2;
static const uint unavailable_state = 20;
static const uint disconnected_state = 30;
static const uint activated_state = 100;
static const uint privacy_flag = 0x1;
static const uint key_mgmt_802_1x = 0x200;

// Changes come in bursts; everything is read once after this long.
static const int reading_delay_ms = 300;
// NetworkManager answers at once; a stuck one must not hang the window.
static const int call_timeout_ms = 1000;
// While the pane is on screen, Wi-Fi networks are looked for this often.
static const int scan_interval_ms = 15000;
static const int dot_size = 12;

static QVariantMap all_properties(const QString& path, const QString& interface)
{
    QDBusMessage asking = QDBusMessage::createMethodCall(service_name, path, properties_interface, QStringLiteral("GetAll"));
    asking << interface;
    const QDBusReply<QVariantMap> answer = QDBusConnection::systemBus().call(asking, QDBus::Block, call_timeout_ms);
    return answer.isValid() ? answer.value() : QVariantMap();
}

static QStringList object_paths(const QVariant& value)
{
    QStringList paths;
    for (const QDBusObjectPath& path : qdbus_cast<QList<QDBusObjectPath>>(value))
    {
        paths << path.path();
    }
    return paths;
}

static QString path_of(const QVariant& value)
{
    const QString path = qdbus_cast<QDBusObjectPath>(value).path();
    return path == QLatin1String("/") ? QString() : path;
}

// A filled circle in the colour of a state, as before each connection.
static QIcon state_dot(const QColor& colour)
{
    QPixmap picture(dot_size * 2, dot_size * 2);
    picture.fill(Qt::transparent);
    QPainter painter(&picture);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(colour.darker(130));
    painter.setBrush(colour);
    painter.drawEllipse(QRectF(1, 1, dot_size * 2 - 2, dot_size * 2 - 2));
    return QIcon(picture);
}

NetworkPane::NetworkPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    qDBusRegisterMetaType<ConnectionSettings>();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    services = new QListWidget;
    services->setFixedSize(source_list_size);
    services->setIconSize(QSize(dot_size, dot_size));
    connect(services, &QListWidget::currentRowChanged, this, &NetworkPane::showChosen);

    status = new QLabel;
    QFont bold = status->font();
    bold.setBold(true);
    status->setFont(bold);
    switch_button = new QPushButton;
    connect(switch_button, &QPushButton::clicked, this, [this]() {
        QDBusMessage change = QDBusMessage::createMethodCall(service_name, manager_path, properties_interface, QStringLiteral("Set"));
        change << manager_interface << QStringLiteral("WirelessEnabled") << QVariant::fromValue(QDBusVariant(!wifi_on));
        QDBusConnection::systemBus().asyncCall(change);
    });
    auto* status_row = new QHBoxLayout;
    status_row->addWidget(status);
    status_row->addStretch();
    status_row->addWidget(switch_button);

    explanation = new QLabel;
    explanation->setWordWrap(true);
    const QFont small = scaled_font(explanation->font(), small_words_scale);
    explanation->setFont(small);

    network_label = new QLabel(word("network_name", "Network Name:"));
    network_choice = new QComboBox;
    network_choice->setMinimumWidth(200);
    connect(network_choice, &QComboBox::activated, this, &NetworkPane::choose);
    forget_button = new QPushButton(word("network_forget", "Forget This Network"));
    connect(forget_button, &QPushButton::clicked, this, &NetworkPane::forgetCurrent);

    address_line = new QLabel;
    router_line = new QLabel;
    address_line->setTextInteractionFlags(Qt::TextSelectableByMouse);
    router_line->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->addRow(network_label, network_choice);
    form->addRow(QString(), forget_button);
    form->addRow(word("network_address", "IP Address:"), address_line);
    form->addRow(word("network_router", "Router:"), router_line);

    auto* box = new QFrame;
    box->setFrameShape(QFrame::StyledPanel);
    box->setFixedSize(detail_box_size);
    auto* box_rows = new QVBoxLayout(box);
    box_rows->setContentsMargins(box_margin_px, box_margin_px, box_margin_px, box_margin_px);
    box_rows->setSpacing(pane_spacing_px);
    box_rows->addLayout(status_row);
    box_rows->addWidget(explanation);
    box_rows->addLayout(form);
    box_rows->addStretch();

    auto* columns = new QHBoxLayout(this);
    columns->setSpacing(list_to_box_px);
    columns->addWidget(services, 0, Qt::AlignTop);
    columns->addWidget(box, 0, Qt::AlignTop);

    reading_soon = new QTimer(this);
    reading_soon->setSingleShot(true);
    reading_soon->setInterval(reading_delay_ms);
    connect(reading_soon, &QTimer::timeout, this, &NetworkPane::readAll);

    scanning = new QTimer(this);
    scanning->setInterval(scan_interval_ms);
    connect(scanning, &QTimer::timeout, this, &NetworkPane::scan);

    // Any property change, device or network coming or going (an empty path means any).
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(service_name, QString(), properties_interface, QStringLiteral("PropertiesChanged"), this,
        SLOT(somethingChanged()));
    for (const char* name : {"DeviceAdded", "DeviceRemoved", "StateChanged"})
    {
        bus.connect(service_name, manager_path, manager_interface, QLatin1String(name), this, SLOT(somethingChanged()));
    }
    for (const char* name : {"AccessPointAdded", "AccessPointRemoved"})
    {
        bus.connect(service_name, QString(), wireless_interface, QLatin1String(name), this, SLOT(somethingChanged()));
    }
    for (const char* name : {"NewConnection", "ConnectionRemoved"})
    {
        bus.connect(service_name, settings_path, settings_interface, QLatin1String(name), this, SLOT(somethingChanged()));
    }

    readAll();
}

QString NetworkPane::word(const char* key, const char* fallback) const
{
    return words.value(QLatin1String(key), QLatin1String(fallback)).toString();
}

void NetworkPane::somethingChanged()
{
    reading_soon->start();
}

void NetworkPane::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scan();
    scanning->start();
}

void NetworkPane::hideEvent(QHideEvent* event)
{
    scanning->stop();
    QWidget::hideEvent(event);
}

void NetworkPane::scan()
{
    for (const Device& device : devices)
    {
        if (device.wifi && wifi_on)
        {
            QDBusMessage asking = QDBusMessage::createMethodCall(service_name, device.path, wireless_interface, QStringLiteral("RequestScan"));
            asking << QVariantMap();
            QDBusConnection::systemBus().asyncCall(asking);
        }
    }
}

void NetworkPane::readAll()
{
    const QVariantMap manager = all_properties(manager_path, manager_interface);
    have_network_manager = !manager.isEmpty();
    wifi_on = manager.value(QStringLiteral("WirelessEnabled")).toBool();

    // Saved Wi-Fi connections, by network name.
    saved.clear();
    QDBusInterface settings(service_name, settings_path, settings_interface, QDBusConnection::systemBus());
    settings.setTimeout(call_timeout_ms);
    const QDBusReply<QList<QDBusObjectPath>> listed = settings.call(QStringLiteral("ListConnections"));
    for (const QDBusObjectPath& connection : listed.value())
    {
        QDBusInterface one(service_name, connection.path(), connection_interface, QDBusConnection::systemBus());
        one.setTimeout(call_timeout_ms);
        const QDBusReply<ConnectionSettings> reply = one.call(QStringLiteral("GetSettings"));
        const QVariantMap wireless = reply.value().value(QStringLiteral("802-11-wireless"));
        const QString name = QString::fromUtf8(wireless.value(QStringLiteral("ssid")).toByteArray());
        if (!name.isEmpty())
        {
            saved.insert(name, connection.path());
        }
    }

    QDBusInterface asking(service_name, manager_path, manager_interface, QDBusConnection::systemBus());
    asking.setTimeout(call_timeout_ms);
    const QDBusReply<QList<QDBusObjectPath>> device_paths = asking.call(QStringLiteral("GetDevices"));

    devices.clear();
    for (const QDBusObjectPath& path : device_paths.value())
    {
        const QVariantMap facts = all_properties(path.path(), device_interface);
        const uint type = facts.value(QStringLiteral("DeviceType")).toUInt();
        if ((type != ethernet_type && type != wifi_type) || !facts.value(QStringLiteral("Managed")).toBool())
        {
            continue;
        }
        Device device;
        device.path = path.path();
        device.wifi = type == wifi_type;
        device.name = device.wifi ? word("network_wifi", "Wi-Fi") : word("network_ethernet", "Ethernet");
        device.state = facts.value(QStringLiteral("State")).toUInt();

        const QString ip4 = path_of(facts.value(QStringLiteral("Ip4Config")));
        if (!ip4.isEmpty() && device.state == activated_state)
        {
            const QVariantMap config = all_properties(ip4, ip4_interface);
            const auto addresses = qdbus_cast<QList<QVariantMap>>(config.value(QStringLiteral("AddressData")));
            if (!addresses.isEmpty())
            {
                device.address = addresses.first().value(QStringLiteral("address")).toString();
            }
            device.router = config.value(QStringLiteral("Gateway")).toString();
        }

        if (device.wifi)
        {
            const QVariantMap radio = all_properties(device.path, wireless_interface);
            const QString active = path_of(radio.value(QStringLiteral("ActiveAccessPoint")));
            QMap<QString, Network> by_name;
            for (const QString& point : object_paths(radio.value(QStringLiteral("AccessPoints"))))
            {
                const QVariantMap seen = all_properties(point, point_interface);
                Network network;
                network.name = QString::fromUtf8(seen.value(QStringLiteral("Ssid")).toByteArray());
                if (network.name.isEmpty())
                {
                    continue;
                }
                network.access_point = point;
                network.strength = seen.value(QStringLiteral("Strength")).toInt();
                const uint wpa = seen.value(QStringLiteral("WpaFlags")).toUInt();
                const uint rsn = seen.value(QStringLiteral("RsnFlags")).toUInt();
                network.secured = (seen.value(QStringLiteral("Flags")).toUInt() & privacy_flag) || wpa || rsn;
                network.enterprise = (wpa | rsn) & key_mgmt_802_1x;
                if (point == active)
                {
                    device.network_name = network.name;
                }
                // The strongest access point stands for the network.
                if (!by_name.contains(network.name) || by_name[network.name].strength < network.strength)
                {
                    by_name.insert(network.name, network);
                }
            }
            device.networks = by_name.values();
            std::sort(device.networks.begin(), device.networks.end(),
                [](const Network& one, const Network& other) { return one.strength > other.strength; });
        }
        devices << device;
    }
    // Wi-Fi first, as on the original.
    std::stable_sort(devices.begin(), devices.end(), [](const Device& one, const Device& other) { return one.wifi && !other.wifi; });

    const int chosen_before = qMax(0, services->currentRow());
    {
        const QSignalBlocker quiet(services);
        services->clear();
        for (const Device& device : devices)
        {
            QString state = word("network_not_connected", "Not Connected");
            QColor colour(0xd9, 0x3b, 0x2f);
            if (device.wifi && !wifi_on)
            {
                state = word("network_off", "Off");
                colour = QColor(0xa0, 0xa0, 0xa0);
            }
            else if (device.state == activated_state)
            {
                state = word("network_connected", "Connected");
                colour = QColor(0x3f, 0xb3, 0x3f);
            }
            else if (device.state > disconnected_state)
            {
                state = word("network_connecting", "Connecting…");
                colour = QColor(0xe8, 0xb3, 0x2c);
            }
            new QListWidgetItem(state_dot(colour), device.name + QStringLiteral("\n") + state, services);
        }
        services->setCurrentRow(qMin(chosen_before, services->count() - 1));
    }
    showChosen();
}

void NetworkPane::showChosen()
{
    const int index = services->currentRow();
    const bool have = index >= 0 && index < devices.size();
    const Device device = have ? devices[index] : Device();
    const bool wifi = have && device.wifi;
    const bool connected = have && device.state == activated_state;

    switch_button->setVisible(wifi);
    switch_button->setText(wifi_on ? word("network_turn_off", "Turn Wi-Fi Off") : word("network_turn_on", "Turn Wi-Fi On"));
    network_label->setVisible(wifi && wifi_on);
    network_choice->setVisible(wifi && wifi_on);
    forget_button->setVisible(wifi && wifi_on);
    forget_button->setEnabled(saved.contains(device.network_name));
    address_line->setText(device.address.isEmpty() ? QStringLiteral("—") : device.address);
    router_line->setText(device.router.isEmpty() ? QStringLiteral("—") : device.router);

    if (!have_network_manager)
    {
        status->setText(QString());
        explanation->setText(word("network_no_manager", "NetworkManager is not running, so connections cannot be shown or changed."));
        return;
    }
    if (!have)
    {
        status->setText(QString());
        explanation->setText(word("network_none", "This computer has no network connection NetworkManager looks after."));
        return;
    }

    const QString status_word = word("network_status", "Status:");
    if (wifi && !wifi_on)
    {
        status->setText(status_word + QLatin1Char(' ') + word("network_off", "Off"));
        explanation->setText(QString());
    }
    else if (connected)
    {
        status->setText(status_word + QLatin1Char(' ') + word("network_connected", "Connected"));
        explanation->setText(wifi ? word("network_wifi_connected", "Wi-Fi is connected to “%1” and has the IP address %2.")
                                        .arg(device.network_name, device.address)
                                  : word("network_cable_connected", "Ethernet is connected and has the IP address %1.")
                                        .arg(device.address));
    }
    else if (device.state <= unavailable_state && !wifi)
    {
        status->setText(status_word + QLatin1Char(' ') + word("network_cable_unplugged", "Cable Unplugged"));
        explanation->setText(word("network_cable_unplugged_note", "Either the cable is not plugged in, or the device at its other end is not responding."));
    }
    else
    {
        status->setText(status_word + QLatin1Char(' ') + word("network_not_connected", "Not Connected"));
        explanation->setText(wifi ? word("network_wifi_not_connected", "Wi-Fi is on but not connected to a network.") : QString());
    }

    if (wifi && wifi_on)
    {
        const QSignalBlocker quiet(network_choice);
        network_choice->clear();
        if (device.network_name.isEmpty())
        {
            network_choice->addItem(word("network_no_network", "No network selected"));
        }
        for (const Network& network : device.networks)
        {
            // A lock before those that need a password.
            const QIcon lock = network.secured
                                   ? QIcon::fromTheme(QStringLiteral("network-wireless-encrypted"),
                                         QIcon::fromTheme(QStringLiteral("changes-prevent")))
                                   : QIcon();
            network_choice->addItem(lock, network.name, network.name);
            if (network.name == device.network_name)
            {
                network_choice->setCurrentIndex(network_choice->count() - 1);
            }
        }
        network_choice->insertSeparator(network_choice->count());
        network_choice->addItem(word("network_join_other", "Join Other Network…"), QStringLiteral("\x01other"));
    }
}

void NetworkPane::choose(int index)
{
    const QString wanted = network_choice->itemData(index).toString();
    if (wanted == QLatin1String("\x01other"))
    {
        joinOther();
        return;
    }
    const int row = services->currentRow();
    if (wanted.isEmpty() || row < 0 || row >= devices.size())
    {
        return;
    }
    for (const Network& network : devices[row].networks)
    {
        if (network.name == wanted)
        {
            join(network);
            return;
        }
    }
}

QString NetworkPane::savedConnectionFor(const QString& network_name) const
{
    return saved.value(network_name);
}

void NetworkPane::addAndActivate(const QMap<QString, QVariantMap>& settings, const QString& device, const QString& point)
{
    QDBusMessage adding = QDBusMessage::createMethodCall(service_name, manager_path, manager_interface, QStringLiteral("AddAndActivateConnection"));
    adding << QVariant::fromValue(settings) << QVariant::fromValue(QDBusObjectPath(device))
           << QVariant::fromValue(QDBusObjectPath(point.isEmpty() ? QStringLiteral("/") : point));
    QDBusConnection::systemBus().asyncCall(adding);
}

void NetworkPane::join(const Network& network)
{
    const Device& device = devices[services->currentRow()];
    const QString known = savedConnectionFor(network.name);
    if (!known.isEmpty())
    {
        QDBusMessage activating = QDBusMessage::createMethodCall(service_name, manager_path, manager_interface, QStringLiteral("ActivateConnection"));
        activating << QVariant::fromValue(QDBusObjectPath(known)) << QVariant::fromValue(QDBusObjectPath(device.path))
                   << QVariant::fromValue(QDBusObjectPath(network.access_point));
        QDBusConnection::systemBus().asyncCall(activating);
        return;
    }
    if (network.enterprise)
    {
        explanation->setText(word("network_enterprise", "“%1” signs in with a user name or a certificate. Set it up with NetworkManager's connection editor (nm-connection-editor).").arg(network.name));
        showChosen();
        return;
    }

    // NetworkManager fills in the rest from the access point.
    ConnectionSettings settings;
    if (network.secured)
    {
        bool agreed = false;
        const QString password = QInputDialog::getText(this, word("network_title", "Network"),
            word("network_password_for", "The Wi-Fi network “%1” requires a password.").arg(network.name), QLineEdit::Password,
            QString(), &agreed);
        if (!agreed)
        {
            showChosen();
            return;
        }
        settings.insert(QStringLiteral("802-11-wireless-security"), QVariantMap{{QStringLiteral("psk"), password}});
    }
    addAndActivate(settings, device.path, network.access_point);
}

void NetworkPane::joinOther()
{
    const int row = services->currentRow();
    if (row < 0 || row >= devices.size())
    {
        return;
    }
    QDialog asking(this);
    asking.setWindowTitle(word("network_join_other", "Join Other Network…"));
    auto* name = new QLineEdit;
    auto* password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(word("network_password_optional", "Leave empty for an open network"));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(word("network_join", "Join"));
    connect(buttons, &QDialogButtonBox::accepted, &asking, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &asking, &QDialog::reject);
    auto* form = new QFormLayout(&asking);
    form->addRow(new QLabel(word("network_join_other_intro", "Enter the name of the network to join, and its password if it has one.")));
    form->addRow(word("network_name", "Network Name:"), name);
    form->addRow(word("network_password", "Password:"), password);
    form->addRow(buttons);
    if (asking.exec() != QDialog::Accepted || name->text().isEmpty())
    {
        showChosen();
        return;
    }

    // A hidden network does not announce itself, so everything is given.
    ConnectionSettings settings;
    settings.insert(QStringLiteral("connection"), QVariantMap{{QStringLiteral("type"), QStringLiteral("802-11-wireless")},
                                                              {QStringLiteral("id"), name->text()}});
    settings.insert(QStringLiteral("802-11-wireless"), QVariantMap{{QStringLiteral("ssid"), name->text().toUtf8()},
                                                                   {QStringLiteral("hidden"), true}});
    if (!password->text().isEmpty())
    {
        settings.insert(QStringLiteral("802-11-wireless-security"), QVariantMap{{QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk")},
                                                                                {QStringLiteral("psk"), password->text()}});
    }
    addAndActivate(settings, devices[row].path, QString());
}

void NetworkPane::forgetCurrent()
{
    const int row = services->currentRow();
    if (row < 0 || row >= devices.size())
    {
        return;
    }
    const QString known = savedConnectionFor(devices[row].network_name);
    if (!known.isEmpty())
    {
        QDBusConnection::systemBus().asyncCall(
            QDBusMessage::createMethodCall(service_name, known, connection_interface, QStringLiteral("Delete")));
    }
}
