// The Network pane: the computer's connections (Wi-Fi and cable) with their
// state and addresses, turning Wi-Fi on and off, choosing a Wi-Fi network,
// joining a hidden one and forgetting a saved one. It talks to
// NetworkManager over the system bus; nothing is kept in the desktop's
// settings.

#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>
#include <QWidget>

class Choices;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

class NetworkPane : public QWidget
{
    Q_OBJECT

  public:
    NetworkPane(Choices& choices, QWidget* parent = nullptr);

  protected:
    // Networks are looked for only while the pane is on screen.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    // Everything is read anew shortly.
    void somethingChanged();

  private:
    // A Wi-Fi network in reach, by name; several access points are one.
    struct Network
    {
        QString name;
        QString access_point;
        int strength = 0;
        bool secured = false;
        // Signs in with a user name or certificate, not a password.
        bool enterprise = false;
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
    void choose(int index);
    void join(const Network& network);
    void joinOther();
    void forgetCurrent();
    void scan();
    void addAndActivate(const QMap<QString, QVariantMap>& settings, const QString& device, const QString& point);
    QString savedConnectionFor(const QString& network_name) const;

    QVariantMap words;
    QString word(const char* key, const char* fallback) const;

    QListWidget* services = nullptr;
    QLabel* status = nullptr;
    QPushButton* switch_button = nullptr;
    QLabel* explanation = nullptr;
    QLabel* network_label = nullptr;
    QComboBox* network_choice = nullptr;
    QPushButton* forget_button = nullptr;
    QLabel* address_line = nullptr;
    QLabel* router_line = nullptr;
    QTimer* reading_soon = nullptr;
    QTimer* scanning = nullptr;

    bool wifi_on = false;
    bool have_network_manager = false;
    QList<Device> devices;
    // Saved connections: Wi-Fi network name -> connection's object path.
    QMap<QString, QString> saved;
};
