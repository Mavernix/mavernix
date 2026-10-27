// The laptop's lid, as UPower reports it. While `handlesLid` is true the
// shell takes logind's "handle-lid-switch" lock, so closing the lid no longer
// sleeps by the system's rule and the shell does what [power] lid_closed
// says. Only a lid UPower reports is taken; without one (a desktop
// computer, a container, no UPower) logind keeps it.

#pragma once

#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QVariantMap>

class LidWatch : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool closed READ isClosed NOTIFY closedChanged)
    Q_PROPERTY(bool handlesLid READ handlesLid WRITE setHandlesLid NOTIFY handlesLidChanged)

  public:
    explicit LidWatch(QObject* parent = nullptr);

    bool isClosed() const { return closed; }
    bool handlesLid() const { return handles_lid; }
    void setHandlesLid(bool handles);

  signals:
    void closedChanged();
    void handlesLidChanged();

  private slots:
    void powerChanged(const QString& interface, const QVariantMap& changed, const QStringList& invalidated);

  private:
    void readLid();
    void holdLid();

    bool present = false;
    bool closed = false;
    bool handles_lid = false;
    // While this is held open, logind leaves the lid to us.
    QDBusUnixFileDescriptor lid_lock;
};
