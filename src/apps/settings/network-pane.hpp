// The Network pane: the computer's connections (Wi-Fi and cable) with their
// state and addresses. For Wi-Fi it lists every network in reach and every
// saved one, with plain buttons to connect, disconnect and forget, and a
// line that says what is happening. It talks to NetworkManager over the
// system bus; nothing is kept in the desktop's settings.

#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class Choices;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

class NetworkPane : public QWidget
{
    Q_OBJECT

  public:
    NetworkPane(Choices& choices, QWidget* parent = nullptr);

    // Asks for a hidden network's name and password and joins it; the Wi-Fi
    // menu's "Join Other Network…" opens the pane with it.
    void joinHidden();

  protected:
    // Networks are looked for only while the pane is on screen.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    // Read again in a moment, or when the pane is next shown.
    void somethingChanged();

  private:
    // A Wi-Fi network in reach or saved, by name; several access points are one.
    struct Network
    {
        QString name;
        // Empty if it is saved but not in reach.
        QString access_point;
        int strength = 0;
        bool secured = false;
        // Signs in with a user name or certificate, not a password.
        bool enterprise = false;
        // The saved connection's object path; empty if never joined.
        QString saved;
        bool connected = false;
    };
    // A device NetworkManager manages: a Wi-Fi card or a cable's port.
    struct Device
    {
        QString path;
        bool wifi = false;
        QString name;
        // NetworkManager's NM_DEVICE_STATE.
        uint state = 0;
        QString address;
        QString router;
        QString network_name;
        QList<Network> networks;
    };

    void readAll();
    void showChosen();
    void showNetworks(const Device& device);
    void showButtons();
    const Device* chosenDevice() const;
    const Network* chosenNetwork() const;
    void connectChosen();
    void disconnectDevice();
    void forgetChosen();
    void scan();
    void addAndActivate(const QMap<QString, QVariantMap>& settings, const QString& device, const QString& point);
    // A line under the list saying what is happening.
    void tell(const QString& text);
    // The line under the status.
    void explain(const QString& text);

    QVariantMap words;
    QString word(const char* key, const char* fallback) const;

    QListWidget* services = nullptr;
    QLabel* status = nullptr;
    QPushButton* switch_button = nullptr;
    QLabel* explanation = nullptr;
    QListWidget* network_list = nullptr;
    QPushButton* connect_button = nullptr;
    QPushButton* forget_button = nullptr;
    QPushButton* hidden_button = nullptr;
    QLabel* progress = nullptr;
    QLabel* address_line = nullptr;
    QLabel* router_line = nullptr;
    QWidget* wifi_part = nullptr;
    QTimer* reading_soon = nullptr;
    QTimer* scanning = nullptr;
    // Something changed while the pane was hidden.
    bool read_when_shown = false;

    bool wifi_on = false;
    bool have_network_manager = false;
    QList<Device> devices;
    // Saved Wi-Fi connections: network name -> connection's object path.
    QMap<QString, QString> saved;
    // The network being joined, to say how it went; empty if none.
    QString joining;
    bool joining_seen_busy = false;
    // The network was saved just now: if joining fails, it is unsaved again,
    // so that the next try asks for the password anew.
    bool joining_is_new = false;
};
