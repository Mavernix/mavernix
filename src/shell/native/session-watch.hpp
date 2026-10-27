// Follows systemd-logind: sleep, and lock/unlock requests for this session.
// A "delay" inhibitor holds sleep back until QML has locked the screen and
// calls `readyToSleep()`, or a timeout passes, so a stuck shell cannot keep
// the machine awake. Without logind (in a container) nothing happens.

#pragma once

#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <QTimer>

class SessionWatch : public QObject
{
    Q_OBJECT

  public:
    explicit SessionWatch(QObject* parent = nullptr);

    Q_INVOKABLE void readyToSleep();
    // Asks logind to suspend the machine; without its leave, nothing happens.
    Q_INVOKABLE void sleep();

  signals:
    void aboutToSleep();
    void wokeUp();
    void lockRequested();
    void unlockRequested();

  private slots:
    void sleepIsNear(bool going_to_sleep);
    void sessionFound(const QDBusObjectPath& path);

  private:
    void holdSleepBack();

    // While this is held open, the login service waits before sleeping.
    QDBusUnixFileDescriptor delay;
    QTimer longestWait;
};
