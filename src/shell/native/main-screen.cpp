#include "main-screen.hpp"

#include <QDebug>
#include <QJsonObject>

MainScreen::MainScreen(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::connected, this, &MainScreen::send);
}

void MainScreen::setName(const QString& name)
{
    if (name == screenName)
    {
        return;
    }
    screenName = name;
    emit nameChanged();
    send();
}

void MainScreen::send()
{
    if (screenName.isEmpty())
    {
        return;
    }
    // Connected on the first name, and again if the connection was lost;
    // `connected` sends it then.
    if (!compositor.isConnected())
    {
        if (!compositor.isConnecting())
        {
            compositor.open();
        }
        return;
    }
    compositor.request("x11-settings/main-screen", {{"name", screenName}}, [](const QJsonObject& answer) {
        if (answer.contains("error"))
        {
            qWarning().noquote() << "Shell.Native: X11 programs are not told the main screen:"
                                 << answer.value("error").toString()
                                 << "(is the compositor's \"x11-settings\" plugin loaded?)";
        }
    });
}
