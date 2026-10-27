#include "compositor-link.hpp"

#include <QJsonDocument>
#include <QJsonObject>

#include "compositor-call.hpp"

static const char* const start_method = "shortcut-capture/start";
static const char* const stop_method = "shortcut-capture/stop";

// How long to wait for the compositor's answer to "stop" before closing
// anyway. Closing gives the shortcuts back just the same.
static const int stop_answer_wait_ms = 500;

CompositorLink::CompositorLink(QObject* parent) : QObject(parent)
{
    stop_timeout.setSingleShot(true);
    stop_timeout.setInterval(stop_answer_wait_ms);
    connect(&stop_timeout, &QTimer::timeout, this, &CompositorLink::closeConnection);

    connect(&socket, &QLocalSocket::connected, this, [this]() { sendRequest(QLatin1String(start_method)); });
    connect(&socket, &QLocalSocket::readyRead, this, &CompositorLink::readAnswers);
    connect(&socket, &QLocalSocket::errorOccurred, this, [this]() {
        if (capturing && !stopping)
        {
            capturing = false;
            emit problem(Problem::Unreachable, socket.errorString());
        }
    });
}

CompositorLink::~CompositorLink()
{
    // No polite goodbye: the compositor gives the shortcuts back as soon as
    // the connection is gone.
    socket.abort();
}

void CompositorLink::startShortcutCapture()
{
    if (capturing)
    {
        return;
    }

    const QString path = qEnvironmentVariable("WAYFIRE_SOCKET");
    if (path.isEmpty())
    {
        emit problem(Problem::NoSession, QString());
        return;
    }

    capturing = true;
    stopping = false;
    received.clear();
    socket.connectToServer(path);
}

void CompositorLink::stopShortcutCapture()
{
    if (!capturing)
    {
        return;
    }
    if (socket.state() != QLocalSocket::ConnectedState)
    {
        // Never got as far as asking, or the connection is gone already.
        closeConnection();
        return;
    }

    stopping = true;
    sendRequest(QLatin1String(stop_method));
    stop_timeout.start();
}

void CompositorLink::sendRequest(const QString& method)
{
    socket.write(compositor_message(method));
}

void CompositorLink::readAnswers()
{
    received.append(socket.readAll());
    QByteArray json;
    while (take_compositor_answer(received, json))
    {
        handleAnswer(json);
    }
}

void CompositorLink::handleAnswer(const QByteArray& json)
{
    if (stopping)
    {
        // The answer to "stop".
        closeConnection();
        return;
    }

    const QJsonObject answer = QJsonDocument::fromJson(json).object();
    if (answer.contains(QStringLiteral("error")))
    {
        capturing = false;
        socket.abort();
        emit problem(Problem::Refused, answer.value(QStringLiteral("error")).toString());
        return;
    }
    emit captureStarted();
}

void CompositorLink::closeConnection()
{
    stop_timeout.stop();
    capturing = false;
    stopping = false;
    socket.abort();
}
