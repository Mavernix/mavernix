#include "active-window.hpp"

#include <QJsonArray>

namespace
{
// Undocumented property names of Wayfire's kde-appmenu plugin (0.11.0).
const QString SERVICE_PROPERTY = QStringLiteral("kde-appmenu-service-name");
const QString PATH_PROPERTY = QStringLiteral("kde-appmenu-object-path");

// Qt applications announce their menu within a few hundred milliseconds;
// the later tries are for slow starts.
const int ASK_AGAIN_DELAYS_MS[] = {150, 400, 1000, 3000};
const int ASK_AGAIN_TRIES = sizeof(ASK_AGAIN_DELAYS_MS) / sizeof(ASK_AGAIN_DELAYS_MS[0]);
} // namespace

ActiveWindow::ActiveWindow(QObject* parent) : QObject(parent)
{
    askAgain.setSingleShot(true);
    connect(&askAgain, &QTimer::timeout, this, &ActiveWindow::askForMenuAddress);

    connect(&compositor, &CompositorSocket::connected, this, &ActiveWindow::startWatching);
    connect(&compositor, &CompositorSocket::eventReceived, this, [this](const QJsonObject& event) {
        if (event.value("event").toString() == "view-focused")
        {
            takeWindow(event.value("view"));
        }
    });

    compositor.open();
}

QString ActiveWindow::appId() const
{
    return currentAppId;
}

QString ActiveWindow::menuService() const
{
    return currentMenuService;
}

QString ActiveWindow::menuPath() const
{
    return currentMenuPath;
}

void ActiveWindow::startWatching()
{
    const QJsonObject wanted{{"events", QJsonArray{"view-focused"}}};
    compositor.request("window-rules/events/watch", wanted, [](const QJsonObject&) {});

    compositor.request(
        "window-rules/get-focused-view", {}, [this](const QJsonObject& answer) { takeWindow(answer.value("info")); });
}

void ActiveWindow::takeWindow(const QJsonValue& window)
{
    // Panels and menus take the keyboard too; the menu bar must keep the
    // application's menu while one of its own menus is open.
    const QJsonObject description = window.toObject();
    const bool isApplicationWindow = description.value("role").toString() == "toplevel";
    if (!window.isNull() && !isApplicationWindow)
    {
        return;
    }

    const int newId = window.isNull() ? -1 : description.value("id").toInt(-1);
    if (newId == windowId)
    {
        return;
    }

    windowId = newId;
    currentAppId = description.value("app-id").toString();
    currentMenuService.clear();
    currentMenuPath.clear();
    emit changed();

    askAgain.stop();
    askAgainCount = 0;
    askForMenuAddress();
}

void ActiveWindow::askForMenuAddress()
{
    if (windowId < 0)
    {
        return;
    }

    const int askedFor = windowId;
    const QJsonObject serviceQuestion{{"id", askedFor}, {"property", SERVICE_PROPERTY}};
    const QJsonObject pathQuestion{{"id", askedFor}, {"property", PATH_PROPERTY}};

    compositor.request("window-rules/get-view-property", serviceQuestion,
        [this, askedFor, pathQuestion](const QJsonObject& serviceAnswer) {
        const QString service = serviceAnswer.value("value").toString();

        compositor.request(
            "window-rules/get-view-property", pathQuestion, [this, askedFor, service](const QJsonObject& pathAnswer) {
            if (askedFor != windowId)
            {
                return;
            }
            setMenuAddress(service, pathAnswer.value("value").toString());
        });
    });
}

void ActiveWindow::setMenuAddress(const QString& service, const QString& path)
{
    const bool found = !service.isEmpty() && !path.isEmpty();
    if (!found)
    {
        if (askAgainCount < ASK_AGAIN_TRIES)
        {
            askAgain.start(ASK_AGAIN_DELAYS_MS[askAgainCount]);
            askAgainCount++;
        }
        return;
    }

    if ((service == currentMenuService) && (path == currentMenuPath))
    {
        return;
    }

    currentMenuService = service;
    currentMenuPath = path;
    emit changed();
}
