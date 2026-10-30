#include "compositor-socket.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>

namespace
{
const qsizetype LENGTH_FIELD_BYTES = 4;
}

CompositorSocket::CompositorSocket(QObject* parent) : QObject(parent)
{
    connect(&socket, &QLocalSocket::connected, this, &CompositorSocket::connected);
    connect(&socket, &QLocalSocket::disconnected, this, [this] {
        // Nothing half-read or awaited carries over to a new connection.
        unread.clear();
        waitingForAnswer.clear();
        emit disconnected();
    });
    connect(&socket, &QLocalSocket::readyRead, this, &CompositorSocket::readAvailableMessages);
    connect(&socket, &QLocalSocket::errorOccurred, this, [this] {
        qWarning().noquote() << "Shell.Native: cannot talk to the compositor at" << socket.fullServerName() << "-"
                             << socket.errorString()
                             << "(is Wayfire's \"ipc\" plugin enabled, and WAYFIRE_SOCKET set?)";
    });
}

void CompositorSocket::open()
{
    const QString path = qEnvironmentVariable("WAYFIRE_SOCKET");
    if (path.isEmpty())
    {
        qWarning() << "Shell.Native: WAYFIRE_SOCKET is not set; the active window's menu cannot be found."
                   << "Wayfire sets it when its \"ipc\" plugin is enabled.";
        return;
    }

    socket.connectToServer(path);
}

bool CompositorSocket::isConnected() const
{
    return socket.state() == QLocalSocket::ConnectedState;
}

bool CompositorSocket::isConnecting() const
{
    return socket.state() == QLocalSocket::ConnectingState;
}

void CompositorSocket::request(const QString& method, const QJsonObject& data, AnswerHandler onAnswer)
{
    if (!isConnected())
    {
        return;
    }

    const QJsonObject message{{"method", method}, {"data", data}};
    const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);

    QByteArray length(LENGTH_FIELD_BYTES, '\0');
    qToLittleEndian<quint32>(static_cast<quint32>(body.size()), length.data());

    socket.write(length);
    socket.write(body);
    waitingForAnswer.enqueue(std::move(onAnswer));
}

void CompositorSocket::readAvailableMessages()
{
    unread += socket.readAll();

    while (unread.size() >= LENGTH_FIELD_BYTES)
    {
        const quint32 body_length = qFromLittleEndian<quint32>(unread.constData());
        if (unread.size() < LENGTH_FIELD_BYTES + body_length)
        {
            return;
        }

        const QByteArray body = unread.mid(LENGTH_FIELD_BYTES, body_length);
        unread.remove(0, LENGTH_FIELD_BYTES + body_length);

        const QJsonDocument document = QJsonDocument::fromJson(body);
        const QJsonObject message = document.isArray() ? QJsonObject{{"list", document.array()}} : document.object();
        if (message.contains("event"))
        {
            emit eventReceived(message);
        }
        else if (!waitingForAnswer.isEmpty())
        {
            const AnswerHandler handler = waitingForAnswer.dequeue();
            handler(message);
        }
    }
}
