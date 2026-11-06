#include "shortcut-pause.hpp"

#include <QDebug>
#include <QJsonObject>

ShortcutPause::ShortcutPause(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::connected, this, &ShortcutPause::send);
    // The compositor switches the shortcuts on again for a closed connection.
    connect(&compositor, &CompositorSocket::disconnected, this, [this] {
        asked = false;
        setHolding(false);
    });
}

void ShortcutPause::setPaused(bool paused)
{
    if (paused == wanted)
    {
        return;
    }
    wanted = paused;
    emit pausedChanged();
    send();
}

void ShortcutPause::setHolding(bool holding)
{
    if (holding == held)
    {
        return;
    }
    held = holding;
    emit holdingChanged();
}

void ShortcutPause::send()
{
    if (wanted == asked)
    {
        return;
    }
    // Connected on the first pause and kept open: closing it would switch
    // the shortcuts on. `connected` sends what is wanted then.
    if (!compositor.isConnected())
    {
        if (!compositor.isConnecting())
        {
            compositor.open();
        }
        return;
    }

    asked = wanted;
    if (wanted)
    {
        compositor.request("shortcut-capture/start", {}, [this](const QJsonObject& answer) {
            if (answer.contains("error"))
            {
                // Not ours to stop; Tab then still comes as a shortcut.
                asked = false;
                qInfo().noquote() << "Shell.Native: the compositor's shortcuts stay on:"
                                  << answer.value("error").toString();
                return;
            }
            // Asked to stop meanwhile: that request is already on its way.
            setHolding(asked);
        });
    }
    else
    {
        setHolding(false);
        compositor.request("shortcut-capture/stop", {}, [](const QJsonObject& answer) {
            if (answer.contains("error"))
            {
                qWarning().noquote() << "Shell.Native: switching the compositor's shortcuts on again failed:"
                                     << answer.value("error").toString();
            }
        });
    }
}
