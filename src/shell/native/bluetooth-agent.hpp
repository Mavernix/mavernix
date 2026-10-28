// BlueZ's pairing agent for the session: when a device being paired needs a
// PIN, a passkey or a confirmation that two codes match, BlueZ asks this
// object, which asks the person through QML (the shell's dialog). Without
// an agent such pairings fail silently, both from the menu and from the
// settings window.
//
// Usage in QML:
//   BluetoothAgent {
//       onAsked: (kind, device, code) => ... answer(true, typed) or answer(false, "")
//   }
// `kind` is "pin" (type a PIN), "passkey" (type a number), "confirm" (do
// the codes match?), "show" (type this code on the device) or "authorize"
// (allow the device to connect); `code` is the code to show, if any.

#pragma once

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>

class BluetoothAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")

  public:
    explicit BluetoothAgent(QObject* parent = nullptr);
    ~BluetoothAgent() override;

    // The person's answer to the last `asked`.
    Q_INVOKABLE void answer(bool accepted, const QString& typed);

  signals:
    void asked(const QString& kind, const QString& device, const QString& code);
    // BlueZ gave up waiting, or the device went away: the question is void.
    void cancelled();

  public slots:
    // org.bluez.Agent1, called by BlueZ.
    Q_SCRIPTABLE void Release();
    Q_SCRIPTABLE QString RequestPinCode(const QDBusObjectPath& device);
    Q_SCRIPTABLE void DisplayPinCode(const QDBusObjectPath& device, const QString& pincode);
    Q_SCRIPTABLE uint RequestPasskey(const QDBusObjectPath& device);
    Q_SCRIPTABLE void DisplayPasskey(const QDBusObjectPath& device, uint passkey, ushort entered);
    Q_SCRIPTABLE void RequestConfirmation(const QDBusObjectPath& device, uint passkey);
    Q_SCRIPTABLE void RequestAuthorization(const QDBusObjectPath& device);
    Q_SCRIPTABLE void AuthorizeService(const QDBusObjectPath& device, const QString& uuid);
    Q_SCRIPTABLE void Cancel();

  private slots:
    // BlueZ restarted: register again.
    void registerWithBlueZ();

  private:
    // Holds BlueZ's call until the person answers.
    void wait(const QString& kind, const QDBusObjectPath& device, const QString& code);
    QString deviceName(const QDBusObjectPath& device) const;

    QDBusMessage pending;
    QString pending_kind;
};
