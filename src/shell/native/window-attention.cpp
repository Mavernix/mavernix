#include "window-attention.hpp"

WindowAttention::WindowAttention(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::connected, this, [this] {
        compositor.request("window-attention/watch", {}, [](const QJsonObject&) {});
    });
    connect(&compositor, &CompositorSocket::eventReceived, this, [this](const QJsonObject& event) {
        if (event.value("event").toString() != "window-attention")
        {
            return;
        }
        const qint64 id = event.value("id").toInteger();
        const bool wants = event.value("wants").toBool();
        // Hints change for other reasons too; only a change in asking counts.
        if (wants == asking.contains(id))
        {
            return;
        }
        if (wants)
        {
            asking.insert(id, event.value("app-id").toString());
        }
        else
        {
            asking.remove(id);
        }
        emit changed();
    });
    compositor.open();
}

QStringList WindowAttention::appIds() const
{
    QStringList app_ids = asking.values();
    app_ids.removeDuplicates();
    return app_ids;
}
