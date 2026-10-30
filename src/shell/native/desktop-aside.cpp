#include "desktop-aside.hpp"

#include <QDebug>

DesktopAside::DesktopAside(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::eventReceived, this, [this](const QJsonObject& event) {
        if (event.value("event").toString() == "desktop-aside-pressed")
        {
            emit pressed(event.value("output").toString());
        }
    });
    connect(&compositor, &CompositorSocket::connected, this, [this] {
        if (!waitingMove.isEmpty())
        {
            send(waitingMove);
            waitingMove = {};
        }
    });
}

void DesktopAside::move(const QString& screenName, int by, int durationMs, const QString& curve)
{
    const QJsonObject question{
        {"output", screenName},
        {"by", by},
        {"duration_ms", durationMs},
        {"curve", curve},
    };
    if (compositor.isConnected())
    {
        send(question);
        return;
    }

    waitingMove = question;
    if (!opened)
    {
        opened = true;
        compositor.open();
    }
}

void DesktopAside::send(const QJsonObject& question)
{
    compositor.request("desktop-aside/move", question, [](const QJsonObject& answer) {
        if (answer.contains("error"))
        {
            qWarning().noquote() << "Shell.Native: the desktop cannot move aside:" << answer.value("error").toString()
                                 << "(is the compositor's \"desktop-aside\" plugin loaded?)";
        }
    });
}
