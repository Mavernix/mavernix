#include "window-attention.hpp"

#include <QDebug>

WindowAttention::WindowAttention(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::connected, this, [this] {
        compositor.request("window-attention/watch", {}, [](const QJsonObject& answer) {
            if (answer.contains("error"))
            {
                qWarning().noquote() << "Shell.Native: windows asking for attention go unseen:"
                                     << answer.value("error").toString()
                                     << "(is the compositor's \"window-attention\" plugin loaded?)";
            }
        });
    });
    // Without the compositor nothing can be known to ask any more.
    connect(&compositor, &CompositorSocket::disconnected, this, [this] {
        if (!asking.isEmpty())
        {
            asking.clear();
            changes += 1;
            emit changed();
        }
    });
    // The compositor tells only of changes: a window starting or stopping to ask.
    connect(&compositor, &CompositorSocket::eventReceived, this, [this](const QJsonObject& event) {
        if (event.value("event").toString() != "window-attention")
        {
            return;
        }
        const qint64 id = event.value("id").toInteger();
        if (event.value("wants").toBool())
        {
            asking.insert(id, event.value("app-id").toString());
        }
        else
        {
            asking.remove(id);
        }
        changes += 1;
        emit changed();
    });
    compositor.open();
}

bool WindowAttention::asks(const QString& appId, int) const
{
    for (const QString& asking_app : asking)
    {
        if (asking_app == appId)
        {
            return true;
        }
    }
    return false;
}

void WindowAttention::answered(const QString& appId)
{
    if (asking.removeIf([&appId](auto entry) { return entry.value() == appId; }) > 0)
    {
        changes += 1;
        emit changed();
    }
}

int WindowAttention::revision() const
{
    return changes;
}
