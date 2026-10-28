#include "desktops.hpp"

#include <QJsonArray>

#include <cmath>

Desktops::Desktops(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::connected, this, &Desktops::startWatching);
    connect(&compositor, &CompositorSocket::eventReceived, this, [this](const QJsonObject& event) {
        if (event.value("event").toString() == "wset-workspace-changed")
        {
            takeScreen(event.value("output-data").toObject());
        }
    });

    compositor.open();
}

int Desktops::count() const
{
    return desktopCount;
}

int Desktops::current() const
{
    return currentDesktop;
}

qreal Desktops::screenWidth() const
{
    return width;
}

void Desktops::startWatching()
{
    const QJsonObject wanted{{"events", QJsonArray{"wset-workspace-changed"}}};
    compositor.request("window-rules/events/watch", wanted, [](const QJsonObject&) {});
    refresh();
}

void Desktops::refresh()
{
    compositor.request("window-rules/list-outputs", {}, [this](const QJsonObject& answer) {
        const QJsonArray screens = answer.value("list").toArray();
        if (!screens.isEmpty())
        {
            takeScreen(screens.first().toObject());
        }
    });
}

void Desktops::takeScreen(const QJsonObject& screen)
{
    const int id = screen.value("id").toInt(-1);
    if ((screenId != -1) && (id != screenId))
    {
        return;
    }
    screenId = id;

    const QJsonObject place = screen.value("workspace").toObject();
    desktopCount = place.value("grid_width").toInt(1);
    currentDesktop = place.value("x").toInt(0);
    width = screen.value("geometry").toObject().value("width").toDouble();
    emit changed();
}

void Desktops::show(int index)
{
    if ((screenId == -1) || (index < 0) || (index >= desktopCount) || (index == currentDesktop))
    {
        return;
    }
    const QJsonObject where{{"x", index}, {"y", 0}, {"output-id", screenId}};
    compositor.request("vswitch/set-workspace", where, [](const QJsonObject&) {});
}

void Desktops::placeWindow(int id, qreal x, qreal y, qreal windowWidth, qreal windowHeight)
{
    const QJsonObject place{{"x", x}, {"y", y}, {"width", windowWidth}, {"height", windowHeight}};
    const QJsonObject wish{{"id", id}, {"geometry", place}};
    compositor.request("window-rules/configure-view", wish, [](const QJsonObject&) {});
}

void Desktops::closeWindow(int id)
{
    compositor.request("window-rules/close-view", QJsonObject{{"id", id}}, [](const QJsonObject&) {});
}

void Desktops::closeGap(int index)
{
    if ((screenId == -1) || (width <= 0) || (index < 0) || (index >= desktopCount) || (desktopCount < 2))
    {
        return;
    }

    // The windows of the first desktop have nowhere to go to the left:
    // there the second desktop's windows join them instead.
    const int first_to_move = std::max(index, 1);
    const int shown = currentDesktop;

    compositor.request("window-rules/list-views", {}, [this, first_to_move, shown](const QJsonObject& answer) {
        for (const QJsonValue& value : answer.value("list").toArray())
        {
            const QJsonObject window = value.toObject();
            if ((window.value("role").toString() != "toplevel") || (window.value("output-id").toInt() != screenId))
            {
                continue;
            }

            // A window belongs to the desktop its middle is on.
            QJsonObject place = window.value("geometry").toObject();
            const double middle = place.value("x").toDouble() + place.value("width").toDouble() / 2;
            const int desktop = shown + int(std::floor(middle / width));
            if (desktop < first_to_move)
            {
                continue;
            }

            place["x"] = place.value("x").toDouble() - width;
            const QJsonObject wish{{"id", window.value("id").toInt()}, {"geometry", place}};
            compositor.request("window-rules/configure-view", wish, [](const QJsonObject&) {});
        }

        // Answers come in request order, so this one follows all the moves.
        const int shown_after = shown >= first_to_move ? shown - 1 : shown;
        const QJsonObject where{{"x", shown_after}, {"y", 0}, {"output-id", screenId}};
        compositor.request("vswitch/set-workspace", where, [this](const QJsonObject&) { emit gapClosed(); });
    });
}
