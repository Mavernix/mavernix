#include "network-pane.hpp"

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
#include <QHBoxLayout>
#include <QIcon>
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
static const uint failed_state = 120;
static const uint privacy_flag = 0x1;
static const uint key_mgmt_802_1x = 0x200;

// Changes come in bursts; everything is read once after this long.
static const int reading_delay_ms = 300;
// NetworkManager answers at once; a stuck one must not hang the window.
static const int call_timeout_ms = 1000;
// While the pane is on screen, Wi-Fi networks are looked for this often.
static const int scan_interval_ms = 15000;
static const int dot_size = 12;
static const int network_list_height_px = 150;
// The window cannot be resized: the connections' list is narrow, and the box
// beside it takes the rest of the pane's width.
static const int services_width_px = 150;
static const int box_width_px = intro_width_px - services_width_px - list_to_box_px;
// Text in the box is broken into lines no wider than this, by hand: a label
// that wraps by itself asks for far more height than it fills.
static const int box_text_px = box_width_px - 2 * box_margin_px - 8;

// The network list keeps a network's name here, the services list nothing.
static const int name_role = Qt::UserRole;

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

static void call_async(const QString& path, const QString& interface, const char* method, const QVariantList& arguments = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(service_name, path, interface, QLatin1String(method));
    message.setArguments(arguments);
    QDBusConnection::systemBus().asyncCall(message);
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

static QIcon signal_icon(int strength)
{
    const char* name = "network-wireless-signal-excellent";
    if (strength < 25)
    {
        name = "network-wireless-signal-weak";
    }
    else if (strength < 50)
    {
        name = "network-wireless-signal-ok";
    }
    else if (strength < 75)
    {
        name = "network-wireless-signal-good";
    }
    return QIcon::fromTheme(QLatin1String(name), QIcon::fromTheme(QStringLiteral("network-wireless")));
}

NetworkPane::NetworkPane(Choices& choices, QWidget* parent) : QWidget(parent)
{
    qDBusRegisterMetaType<ConnectionSettings>();
    qDBusRegisterMetaType<QList<QVariantMap>>();
    words = choices.configTable(QStringLiteral("settings")).value(QStringLiteral("words")).toMap();

    services = new QListWidget;
    services->setFixedWidth(services_width_px);
    services->setIconSize(QSize(dot_size, dot_size));
    connect(services, &QListWidget::currentRowChanged, this, &NetworkPane::showChosen);

    status = new QLabel;
    QFont bold = status->font();
    bold.setBold(true);
    status->setFont(bold);
    switch_button = new QPushButton;
    connect(switch_button, &QPushButton::clicked, this, [this]() {
        call_async(manager_path, properties_interface, "Set",
            {manager_interface, QStringLiteral("WirelessEnabled"), QVariant::fromValue(QDBusVariant(!wifi_on))});
    });
    auto* status_row = new QHBoxLayout;
    status_row->addWidget(status);
    status_row->addStretch();
    status_row->addWidget(switch_button);

    explanation = new QLabel;
    const QFont small = scaled_font(explanation->font(), small_words_scale);
    explanation->setFont(small);

    // Every network in reach and every saved one, each saying what it is.
    network_list = new QListWidget;
    network_list->setFixedHeight(network_list_height_px);
    network_list->setIconSize(QSize(16, 16));
    connect(network_list, &QListWidget::currentRowChanged, this, &NetworkPane::showButtons);
    connect(network_list, &QListWidget::itemDoubleClicked, this, &NetworkPane::connectChosen);

    // One button connects to the chosen network, or disconnects from it if
    // it is the one in use: three buttons fit the box where four did not.
    connect_button = new QPushButton(word("network_connect", "Connect"));
    forget_button = new QPushButton(word("network_forget", "Forget"));
    hidden_button = new QPushButton(word("network_join_hidden", "Hidden Network…"));
    connect(connect_button, &QPushButton::clicked, this, [this]() {
        const Network* chosen = chosenNetwork();
        if (chosen != nullptr && chosen->connected)
        {
            disconnectDevice();
        }
        else
        {
            connectChosen();
        }
    });
    connect(forget_button, &QPushButton::clicked, this, &NetworkPane::forgetChosen);
    connect(hidden_button, &QPushButton::clicked, this, &NetworkPane::joinHidden);
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(hidden_button);
    buttons->addStretch();
    buttons->addWidget(forget_button);
    buttons->addWidget(connect_button);

    progress = new QLabel;
    progress->setFont(small);

    wifi_part = new QWidget;
    auto* wifi_rows = new QVBoxLayout(wifi_part);
    wifi_rows->setContentsMargins(0, 0, 0, 0);
    wifi_rows->addWidget(network_list);
    wifi_rows->addLayout(buttons);
    wifi_rows->addWidget(progress);

    address_line = new QLabel;
    router_line = new QLabel;
    address_line->setTextInteractionFlags(Qt::TextSelectableByMouse);
    router_line->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* addresses = new QFormLayout;
    addresses->setLabelAlignment(Qt::AlignRight);
    addresses->addRow(word("network_address", "IP Address:"), address_line);
    addresses->addRow(word("network_router", "Router:"), router_line);

    auto* box = new QFrame;
    box->setFrameShape(QFrame::StyledPanel);
    box->setFixedWidth(box_width_px);
    auto* box_rows = new QVBoxLayout(box);
    box_rows->setContentsMargins(box_margin_px, box_margin_px, box_margin_px, box_margin_px);
    box_rows->setSpacing(10);
    box_rows->addLayout(status_row);
    box_rows->addWidget(explanation);
    box_rows->addWidget(wifi_part);
    box_rows->addLayout(addresses);
    box_rows->addStretch();

    auto* columns = new QHBoxLayout(this);
    // The list and the box fill the pane's width; a margin would push them out of the window.
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(list_to_box_px);
    columns->addWidget(services);
    columns->addWidget(box);

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
    return words.value(QLatin1String(key), QString::fromUtf8(fallback)).toString();
}

void NetworkPane::somethingChanged()
{
    // Reading takes a call per network and per saved connection, so a hidden
    // pane waits until it is shown, unless it has to say how joining went.
    if (!isVisible() && joining.isEmpty())
    {
        read_when_shown = true;
        return;
    }
    // Not restarted: signal strengths change all the time, and the list would never be read.
    if (!reading_soon->isActive())
    {
        reading_soon->start();
    }
}

void NetworkPane::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (read_when_shown)
    {
        read_when_shown = false;
        readAll();
    }
    scanning->start();
    // Look at once rather than after the first interval.
    scan();
}

void NetworkPane::scan()
{
    for (const Device& device : devices)
    {
        if (device.wifi && wifi_on)
        {
            call_async(device.path, wireless_interface, "RequestScan", {QVariantMap()});
        }
    }
}

void NetworkPane::hideEvent(QHideEvent* event)
{
    scanning->stop();
    QWidget::hideEvent(event);
}

void NetworkPane::tell(const QString& text)
{
    progress->setText(in_lines_of(text, progress->fontMetrics(), box_text_px));
}

void NetworkPane::explain(const QString& text)
{
    explanation->setText(in_lines_of(text, explanation->fontMetrics(), box_text_px));
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
                network.connected = point == active && device.state == activated_state;
                if (point == active)
                {
                    device.network_name = network.name;
                }
                // The strongest access point stands for the network, which is in use if any of them is.
                const bool connected = network.connected || by_name.value(network.name).connected;
                if (!by_name.contains(network.name) || by_name[network.name].strength < network.strength)
                {
                    by_name.insert(network.name, network);
                }
                by_name[network.name].connected = connected;
            }
            // Saved networks stay listed when out of reach, so they can be forgotten.
            for (auto known = saved.begin(); known != saved.end(); ++known)
            {
                by_name[known.key()].name = known.key();
                by_name[known.key()].saved = known.value();
            }
            device.networks = by_name.values();
            // The one in use, then those in reach by strength, then saved ones out of reach.
            std::sort(device.networks.begin(), device.networks.end(), [](const Network& one, const Network& other) {
                if (one.connected != other.connected)
                {
                    return one.connected;
                }
                if (one.access_point.isEmpty() != other.access_point.isEmpty())
                {
                    return !one.access_point.isEmpty();
                }
                return one.strength != other.strength ? one.strength > other.strength
                                                      : one.name.localeAwareCompare(other.name) < 0;
            });

            // Say how joining went: once the card was busy, it either connects
            // to the network or gives up.
            if (!joining.isEmpty())
            {
                if (device.state > disconnected_state && device.state < activated_state)
                {
                    joining_seen_busy = true;
                }
                else if (device.state == activated_state && device.network_name == joining)
                {
                    tell(word("network_joined", "Connected to “%1”.").arg(joining));
                    joining.clear();
                }
                else if (joining_seen_busy && (device.state == failed_state || device.state == disconnected_state))
                {
                    if (joining_is_new && saved.contains(joining))
                    {
                        call_async(saved.value(joining), connection_interface, "Delete");
                    }
                    tell(word("network_join_failed", "Could not connect to “%1”. If it has a password, check it and try again.").arg(joining));
                    joining.clear();
                }
            }
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

const NetworkPane::Device* NetworkPane::chosenDevice() const
{
    const int index = services->currentRow();
    return (index >= 0 && index < devices.size()) ? &devices[index] : nullptr;
}

const NetworkPane::Network* NetworkPane::chosenNetwork() const
{
    const Device* device = chosenDevice();
    const QListWidgetItem* row = network_list->currentItem();
    if (device == nullptr || row == nullptr)
    {
        return nullptr;
    }
    const QString name = row->data(name_role).toString();
    for (const Network& network : device->networks)
    {
        if (network.name == name)
        {
            return &network;
        }
    }
    return nullptr;
}

void NetworkPane::showChosen()
{
    const Device* device = chosenDevice();
    const bool wifi = device != nullptr && device->wifi;
    const bool connected = device != nullptr && device->state == activated_state;

    switch_button->setVisible(wifi);
    switch_button->setText(wifi_on ? word("network_turn_off", "Turn Wi-Fi Off") : word("network_turn_on", "Turn Wi-Fi On"));
    wifi_part->setVisible(wifi && wifi_on);
    address_line->setText(connected ? device->address : QString());
    router_line->setText(connected ? device->router : QString());

    const QString status_word = word("network_status", "Status:") + QLatin1Char(' ');
    if (!have_network_manager)
    {
        status->setText(QString());
        explain(word("network_no_manager", "NetworkManager is not running, so connections cannot be shown or changed."));
        return;
    }
    if (device == nullptr)
    {
        status->setText(QString());
        explain(word("network_none", "This computer has no network connection NetworkManager looks after."));
        return;
    }

    if (wifi && !wifi_on)
    {
        status->setText(status_word + word("network_off", "Off"));
        explain(word("network_wifi_off", "Turn Wi-Fi on to see the networks around you."));
    }
    else if (connected)
    {
        status->setText(status_word + word("network_connected", "Connected"));
        explain(wifi ? word("network_wifi_connected", "Connected to “%1”. Choose another network below and click Connect to switch.")
                                        .arg(device->network_name)
                                  : word("network_cable_connected", "Connected by cable."));
    }
    else if (device->state <= unavailable_state && !wifi)
    {
        status->setText(status_word + word("network_cable_unplugged", "Cable Unplugged"));
        explain(word("network_cable_unplugged_note", "Plug in a network cable, or check the device at its other end."));
    }
    else
    {
        status->setText(status_word + word("network_not_connected", "Not Connected"));
        explain(wifi ? word("network_wifi_choose", "Choose a network below and click Connect.") : QString());
    }

    if (wifi && wifi_on)
    {
        showNetworks(*device);
    }
}

void NetworkPane::showNetworks(const Device& device)
{
    const QListWidgetItem* before = network_list->currentItem();
    const QString chosen_before = before ? before->data(name_role).toString() : QString();
    const QSignalBlocker quiet(network_list);
    network_list->clear();
    for (const Network& network : device.networks)
    {
        // What it is, in words, after its name.
        QStringList notes;
        if (network.connected)
        {
            notes << word("network_note_connected", "Connected");
        }
        else if (network.name == joining)
        {
            notes << word("network_note_connecting", "Connecting…");
        }
        else if (!network.saved.isEmpty())
        {
            notes << word("network_note_saved", "Saved");
        }
        if (network.access_point.isEmpty())
        {
            notes << word("network_note_away", "not in range");
        }
        const QString label = notes.isEmpty() ? network.name
                                              : QStringLiteral("%1 (%2)").arg(network.name, notes.join(QStringLiteral(", ")));
        auto* row = new QListWidgetItem(network.access_point.isEmpty() ? QIcon() : signal_icon(network.strength), label, network_list);
        row->setData(name_role, network.name);
        if (network.connected)
        {
            QFont font = row->font();
            font.setBold(true);
            row->setFont(font);
        }
        if (network.name == chosen_before)
        {
            network_list->setCurrentItem(row);
        }
    }
    showButtons();
}

void NetworkPane::showButtons()
{
    const Network* network = chosenNetwork();
    const Device* device = chosenDevice();
    const bool in_use = network != nullptr && network->connected && device != nullptr && device->state == activated_state;
    connect_button->setText(in_use ? word("network_disconnect", "Disconnect") : word("network_connect", "Connect"));
    connect_button->setEnabled(in_use || (network != nullptr && !network->access_point.isEmpty()));
    forget_button->setEnabled(network != nullptr && !network->saved.isEmpty());
}

void NetworkPane::addAndActivate(const QMap<QString, QVariantMap>& settings, const QString& device, const QString& point)
{
    call_async(manager_path, manager_interface, "AddAndActivateConnection",
        {QVariant::fromValue(settings), QVariant::fromValue(QDBusObjectPath(device)),
         QVariant::fromValue(QDBusObjectPath(point.isEmpty() ? QStringLiteral("/") : point))});
}

void NetworkPane::connectChosen()
{
    const Network* chosen = chosenNetwork();
    const Device* device = chosenDevice();
    if (chosen == nullptr || device == nullptr || chosen->connected || chosen->access_point.isEmpty())
    {
        return;
    }
    const Network network = *chosen;
    // The list is read anew while the password is asked for, so the device is kept by its path.
    const QString device_path = device->path;
    if (network.enterprise && network.saved.isEmpty())
    {
        tell(word("network_enterprise", "“%1” signs in with a user name or a certificate. Set it up with NetworkManager's connection editor (nm-connection-editor).").arg(network.name));
        return;
    }

    joining = network.name;
    joining_seen_busy = false;
    joining_is_new = network.saved.isEmpty();
    tell(word("network_joining", "Connecting to “%1”…").arg(network.name));
    if (!network.saved.isEmpty())
    {
        call_async(manager_path, manager_interface, "ActivateConnection",
            {QVariant::fromValue(QDBusObjectPath(network.saved)), QVariant::fromValue(QDBusObjectPath(device_path)),
             QVariant::fromValue(QDBusObjectPath(network.access_point))});
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
            joining.clear();
            tell(QString());
            return;
        }
        settings.insert(QStringLiteral("802-11-wireless-security"), QVariantMap{{QStringLiteral("psk"), password}});
    }
    addAndActivate(settings, device_path, network.access_point);
}

void NetworkPane::disconnectDevice()
{
    const Device* device = chosenDevice();
    if (device != nullptr)
    {
        call_async(device->path, device_interface, "Disconnect");
        tell(QString());
    }
}

void NetworkPane::forgetChosen()
{
    const Network* network = chosenNetwork();
    if (network == nullptr || network->saved.isEmpty())
    {
        return;
    }
    call_async(network->saved, connection_interface, "Delete");
    tell(word("network_forgotten", "“%1” is forgotten: its password is no longer kept, and the computer won't join it by itself.")
             .arg(network->name));
}

void NetworkPane::joinHidden()
{
    const Device* device = chosenDevice();
    if (device == nullptr)
    {
        return;
    }
    // The list is read anew while the dialog is open, so the device is kept by its path.
    const QString device_path = device->path;
    QDialog asking(this);
    asking.setWindowTitle(word("network_join_hidden", "Hidden Network…"));
    auto* name = new QLineEdit;
    auto* password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(word("network_password_optional", "Leave empty for an open network"));
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(word("network_join", "Join"));
    connect(buttons, &QDialogButtonBox::accepted, &asking, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &asking, &QDialog::reject);
    auto* form = new QFormLayout(&asking);
    form->addRow(new QLabel(word("network_join_hidden_intro", "A hidden network doesn't show up in the list. Enter its name, and its password if it has one.")));
    form->addRow(word("network_name", "Network Name:"), name);
    form->addRow(word("network_password", "Password:"), password);
    form->addRow(buttons);
    if (asking.exec() != QDialog::Accepted || name->text().isEmpty())
    {
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
    joining = name->text();
    joining_seen_busy = false;
    // If a connection by that name is already saved, a failed try must not unsave it.
    joining_is_new = !saved.contains(joining);
    tell(word("network_joining", "Connecting to “%1”…").arg(joining));
    addAndActivate(settings, device_path, QString());
}
