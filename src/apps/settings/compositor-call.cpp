#include "compositor-call.hpp"

#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>
#include <QtEndian>

#include <memory>

static const int length_prefix_bytes = 4;
// A compositor that is up answers at once.
static const int answer_wait_ms = 2000;

QByteArray compositor_message(const QString& method, const QJsonObject& data)
{
    QJsonObject request;
    request.insert(QStringLiteral("method"), method);
    request.insert(QStringLiteral("data"), data);
    const QByteArray body = QJsonDocument(request).toJson(QJsonDocument::Compact);

    QByteArray message(length_prefix_bytes, '\0');
    qToLittleEndian<quint32>(quint32(body.size()), message.data());
    message.append(body);
    return message;
}

bool take_compositor_answer(QByteArray& received, QByteArray& json)
{
    if (received.size() < length_prefix_bytes)
    {
        return false;
    }
    const quint32 length = qFromLittleEndian<quint32>(received.constData());
    if (received.size() < qsizetype(length_prefix_bytes + length))
    {
        return false;
    }
    json = received.mid(length_prefix_bytes, length);
    received.remove(0, length_prefix_bytes + length);
    return true;
}

void ask_compositor(const QString& method, QObject* context,
    std::function<void(const QJsonObject& answer, const QString& problem)> answered)
{
    const QString path = qEnvironmentVariable("WAYFIRE_SOCKET");
    if (path.isEmpty())
    {
        answered(QJsonObject(), QStringLiteral("WAYFIRE_SOCKET is not set: not inside the desktop's session"));
        return;
    }

    auto* socket = new QLocalSocket(context);
    auto* give_up = new QTimer(socket);
    auto received = std::make_shared<QByteArray>();
    // Whichever comes first ends the call; the others then find it ended.
    auto finish = [socket, answered](const QJsonObject& answer, const QString& problem) {
        if (socket->property("finished").toBool())
        {
            return;
        }
        socket->setProperty("finished", true);
        socket->abort();
        socket->deleteLater();
        answered(answer, problem);
    };

    QObject::connect(
        socket, &QLocalSocket::connected, socket, [socket, method]() { socket->write(compositor_message(method)); });
    QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, received, finish]() {
        received->append(socket->readAll());
        QByteArray json;
        if (!take_compositor_answer(*received, json))
        {
            return;
        }
        const QJsonObject answer = QJsonDocument::fromJson(json).object();
        if (answer.contains(QStringLiteral("error")))
        {
            finish(QJsonObject(), answer.value(QStringLiteral("error")).toString());
            return;
        }
        finish(answer, QString());
    });
    QObject::connect(socket, &QLocalSocket::errorOccurred, socket,
        [socket, finish]() { finish(QJsonObject(), socket->errorString()); });

    give_up->setSingleShot(true);
    QObject::connect(give_up, &QTimer::timeout, socket,
        [finish]() { finish(QJsonObject(), QStringLiteral("the compositor did not answer")); });
    give_up->start(answer_wait_ms);

    socket->connectToServer(path);
}
