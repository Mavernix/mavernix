// The laptop's lid, as UPower reports it. While `handlesLid` is true the
// shell takes logind's "handle-lid-switch" lock, so closing the lid no longer
// sleeps by the system's rule and the shell does what [power] lid_closed
// says. Only a lid UPower reports is taken; without one (a desktop
// computer, a container, no UPower) logind keeps it.
//
// `docked` is logind's word, asked as the lid closes: a dock or a second
// screen is attached, and logind would leave the lid alone (HandleLidSwitchDocked).

#pragma once

#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QVariantMap>

class LidWatch : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool closed READ isClosed NOTIFY closedChanged)
    Q_PROPERTY(bool docked READ isDocked NOTIFY closedChanged)
    Q_PROPERTY(bool handlesLid READ handlesLid WRITE setHandlesLid NOTIFY handlesLidChanged)

  public:
    explicit LidWatch(QObject* parent = nullptr);

    bool isClosed() const { return closed; }
    bool isDocked() const { return docked; }
    bool handlesLid() const { return handles_lid; }
    void setHandlesLid(bool handles);

  signals:
    void closedChanged();
    void handlesLidChanged();

  private slots:
    void powerChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);

  private:
    // `announce` false: the state the lid is found in at the start (a docked
    // laptop's shut lid) is not news, only what it does from then on.
    void readLid(bool announce);
    void holdLid();
    // A close is told once logind has said whether the laptop is docked.
    void changeLid(bool now_closed);

    bool present = false;
    bool closed = false;
    bool docked = false;
    bool handles_lid = false;
    // Counts the lid's moves, so a late answer about the dock is dropped.
    int lid_moves = 0;
    // While this is held open, logind leaves the lid to us.
    QDBusUnixFileDescriptor lid_lock;
};
