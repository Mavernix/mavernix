// A connection to Wayfire's "ipc" control socket ($WAYFIRE_SOCKET).
// Each message is a 4-byte little-endian length followed by JSON. Answers
// come in request order; events, interleaved, have an "event" field.

#pragma once

#include <functional>

#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QQueue>

class CompositorSocket : public QObject
{
    Q_OBJECT

  public:
    using AnswerHandler = std::function<void(const QJsonObject& answer)>;

    explicit CompositorSocket(QObject* parent = nullptr);

    // Separate from the constructor: a local socket may connect at once,
    // before the caller listens for `connected`.
    void open();
    bool isConnected() const;
    bool isConnecting() const;

    // An answer that is a list arrives as an object with the field "list".
    void request(const QString& method, const QJsonObject& data, AnswerHandler onAnswer);

  signals:
    void connected();
    // The compositor has gone away (or was never reached); open() tries again.
    void disconnected();
    // Only for events asked for, such as with `window-rules/events/watch`.
    void eventReceived(const QJsonObject& event);

  private:
    void readAvailableMessages();

    QLocalSocket socket;
    QByteArray unread;
    // Oldest first, matching the order answers arrive in.
    QQueue<AnswerHandler> waitingForAnswer;
};
