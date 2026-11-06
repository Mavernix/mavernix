#include "shortcut-pause.hpp"

#include <QDebug>
#include <QJsonObject>
#include <QTimer>

ShortcutPause::ShortcutPause(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::connected, this, &ShortcutPause::send);
    // The compositor switches the shortcuts on again for a closed connection.
    connect(&compositor, &CompositorSocket::disconnected, this, [this] { asked = false; });
    // Connected before the first pause, so the first Tab is not late.
    QTimer::singleShot(0, &compositor, &CompositorSocket::open);
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

void ShortcutPause::send()
{
    if (wanted == asked)
    {
        return;
    }
    // Kept open: closing it would switch the shortcuts on. If it is not
    // there yet, `connected` sends what is wanted then.
    if (!compositor.isConnected())
    {
        if (!compositor.isConnecting())
        {
            compositor.open();
        }
        return;
    }

    asked = wanted;
    const int sent = ++requestNumber;
    if (wanted)
    {
        compositor.request("shortcut-capture/start", {}, [this, sent](const QJsonObject& answer) {
            if (!answer.contains("error"))
            {
                return;
            }
            // Not ours to stop; Tab then still comes as a shortcut. A later
            // request has its own answer.
            if (sent == requestNumber)
            {
                asked = false;
            }
            qInfo().noquote() << "Shell.Native: the compositor's shortcuts stay on:"
                              << answer.value("error").toString();
        });
    }
    else
    {
        compositor.request("shortcut-capture/stop", {}, [](const QJsonObject& answer) {
            if (answer.contains("error"))
            {
                qWarning().noquote() << "Shell.Native: switching the compositor's shortcuts on again failed:"
                                     << answer.value("error").toString();
            }
        });
    }
}
