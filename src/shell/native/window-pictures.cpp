#include "window-pictures.hpp"

#include <algorithm>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QPainter>
#include <QTimer>
#include <QUrl>

WindowPictures::WindowPictures(QObject* parent) : QObject(parent)
{
    connect(&compositor, &CompositorSocket::connected, this, &WindowPictures::startWatching);
    connect(&compositor, &CompositorSocket::eventReceived, this, [this](const QJsonObject& event) {
        const QJsonObject window = event.value("view").toObject();
        if (event.value("event").toString() == "view-unmapped")
        {
            forgetWindow(window.value("id").toInt(-1));
        }
        else
        {
            takeWindow(window);
        }
    });

    // Opened once QML has had the chance to listen and to set the folder.
    QTimer::singleShot(0, &compositor, &CompositorSocket::open);
}

int WindowPictures::revision() const
{
    return currentRevision;
}

void WindowPictures::startWatching()
{
    const QJsonObject wanted{{"events",
        QJsonArray{"view-mapped", "view-unmapped", "view-minimized", "view-title-changed", "view-app-id-changed"}}};
    compositor.request("window-rules/events/watch", wanted, [](const QJsonObject&) {});

    compositor.request("window-rules/list-views", {}, [this](const QJsonObject& answer) {
        for (const QJsonValue& window : answer.value("list").toArray())
        {
            takeWindow(window.toObject());
        }
    });
}

void WindowPictures::takeWindow(const QJsonObject& description)
{
    const int id = description.value("id").toInt(-1);
    // Panels, backgrounds and menus are "views" to the compositor too.
    if ((id < 0) || (description.value("role").toString() != "toplevel"))
    {
        return;
    }

    Window& window = windows[id];
    window.appId = description.value("app-id").toString();
    window.title = description.value("title").toString();
    const bool was_minimized = window.minimized;
    window.minimized = description.value("minimized").toBool();

    if (window.minimized && !was_minimized)
    {
        window.picture = Picture::Coming;
        QTimer::singleShot(delayAfterMinimizing, this, [this, id] { askForPicture(id); });
    }
    else if (!window.minimized && was_minimized)
    {
        window.picture = Picture::None;
        window.tileBadge.clear();
        removePictures(id);
    }

    changed();
}

void WindowPictures::forgetWindow(int id)
{
    if (windows.remove(id) > 0)
    {
        removePictures(id);
        changed();
    }
}

void WindowPictures::askForPicture(int id)
{
    if (!windows.contains(id) || !windows[id].minimized)
    {
        return;
    }

    QDir().mkpath(picturesFolder);
    const QJsonObject question{{"id", id}, {"path", windowPicturePath(id)}, {"largest_side", tileSide}};
    compositor.request("window-pictures/write", question, [this, id](const QJsonObject& answer) {
        if (!windows.contains(id) || !windows[id].minimized)
        {
            return;
        }

        const bool written = answer.value("result").toString() == "ok";
        if (!written)
        {
            qWarning().noquote() << "Shell.Native: the compositor wrote no picture of window" << id << "-"
                                 << answer.value("error").toString()
                                 << "(is the \"window-pictures\" plugin in its plugin list?)";
        }

        windows[id].picture = written ? Picture::Ready : Picture::None;
        windows[id].tileBadge.clear();
        changed();
    });
}

int WindowPictures::find(const QString& appId, const QString& title, int rank) const
{
    int earlier = 0;
    for (auto window = windows.constBegin(); window != windows.constEnd(); ++window)
    {
        if ((window->appId != appId) || (window->title != title))
        {
            continue;
        }

        if (earlier == rank)
        {
            return window.key();
        }

        earlier += 1;
    }

    return -1;
}

bool WindowPictures::isComing(const QString& appId, const QString& title, int rank) const
{
    const int id = find(appId, title, rank);
    return (id >= 0) && (windows[id].picture == Picture::Coming);
}

QString WindowPictures::tileFor(const QString& appId, const QString& title, int rank, const QString& badgeIcon)
{
    const int id = find(appId, title, rank);
    if ((id < 0) || (windows[id].picture != Picture::Ready))
    {
        return {};
    }

    // A space stands for "no badge", to tell it from "not composed yet".
    const QString badge = badgeIcon.isEmpty() ? QStringLiteral(" ") : badgeIcon;
    if (windows[id].tileBadge != badge)
    {
        if (!composeTile(id, badgeIcon))
        {
            return {};
        }

        windows[id].tileBadge = badge;
    }

    // A new address per revision keeps QML from showing a cached picture.
    QUrl address = QUrl::fromLocalFile(tilePicturePath(id));
    address.setQuery(QString::number(currentRevision));
    return address.toString();
}

bool WindowPictures::composeTile(int id, const QString& badgeIcon) const
{
    const QImage window(windowPicturePath(id));
    if (window.isNull() || (tileSide <= 0))
    {
        return false;
    }

    QImage tile(tileSide, tileSide, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);
    QPainter painter(&tile);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QSize fitted = window.size().scaled(tileSide, tileSide, Qt::KeepAspectRatio);
    const QRect place(
        (tileSide - fitted.width()) / 2, (tileSide - fitted.height()) / 2, fitted.width(), fitted.height());
    painter.drawImage(place, window);

    const int badge_side = qRound(badgeSideShare * tileSide);
    if ((badge_side > 0) && !badgeIcon.isEmpty())
    {
        const QIcon icon = badgeIcon.startsWith('/') ? QIcon(badgeIcon) : QIcon::fromTheme(badgeIcon);
        const QPixmap badge = icon.pixmap(badge_side, badge_side);
        painter.drawPixmap(QRect(tileSide - badge_side, tileSide - badge_side, badge_side, badge_side), badge);
    }

    painter.end();
    return tile.save(tilePicturePath(id));
}

QString WindowPictures::windowPicturePath(int id) const
{
    return QStringLiteral("%1/%2.png").arg(picturesFolder).arg(id);
}

QString WindowPictures::tilePicturePath(int id) const
{
    return QStringLiteral("%1/%2-tile.png").arg(picturesFolder).arg(id);
}

void WindowPictures::removePictures(int id) const
{
    QFile::remove(windowPicturePath(id));
    QFile::remove(tilePicturePath(id));
}

void WindowPictures::changed()
{
    currentRevision += 1;
    emit revisionChanged();
}

void WindowPictures::takeOverview(int largestSide)
{
    overviewNumber += 1;
    const int this_overview = overviewNumber;
    QDir().mkpath(picturesFolder);

    // Screens and windows are asked on the same connection, so the answers
    // describe the same moment.
    compositor.request("window-rules/list-outputs", {}, [this](const QJsonObject& answer) {
        overviewScreens.clear();
        overviewDesktops = QVariantMap();
        for (const QJsonValue& value : answer.value("list").toArray())
        {
            const QJsonObject screen = value.toObject();
            const QJsonObject shown = screen.value("workspace").toObject();
            const QVariantMap facts{
                {"current", shown.value("x").toInt()},
                {"count", shown.value("grid_width").toInt(1)},
                {"screenWidth", screen.value("geometry").toObject().value("width").toDouble()},
            };
            overviewScreens.insert(screen.value("id").toInt(), facts);
            if (overviewDesktops.isEmpty())
            {
                overviewDesktops = facts;
            }
        }
    });

    compositor.request("window-rules/list-views", {}, [this, largestSide, this_overview](const QJsonObject& answer) {
        overviewWindows.clear();
        overviewPicturesAwaited = 0;

        for (const QJsonValue& value : answer.value("list").toArray())
        {
            const QJsonObject window = value.toObject();
            // Panels and menus are "views" too.
            if ((window.value("role").toString() != "toplevel") || !window.value("mapped").toBool() ||
                window.value("minimized").toBool())
            {
                continue;
            }

            const int id = window.value("id").toInt();
            const QJsonObject place = window.value("geometry").toObject();

            // A window belongs to the desktop its middle is on.
            const QVariantMap screen = overviewScreens.value(window.value("output-id").toInt());
            const double screen_width = screen.value("screenWidth").toDouble();
            int desktop = screen.value("current").toInt();
            if (screen_width > 0)
            {
                const double middle = place.value("x").toDouble() + place.value("width").toDouble() / 2;
                desktop += int(std::floor(middle / screen_width));
                desktop = std::clamp(desktop, 0, std::max(0, screen.value("count").toInt() - 1));
            }
            // Raw pixels are much faster for the compositor to write.
            const QString path = QStringLiteral("%1/overview-%2.rgba").arg(picturesFolder).arg(id);

            overviewPicturesAwaited += 1;
            const QJsonObject question{{"id", id}, {"path", path}, {"largest_side", largestSide}};
            compositor.request("window-pictures/write", question,
                [this, window, place, id, path, this_overview, desktop](const QJsonObject& written) {
                if (written.value("result").toString() == "ok")
                {
                    overviewWindows.append(QVariantMap{
                        {"id", id},
                        {"appId", window.value("app-id").toString()},
                        {"title", window.value("title").toString()},
                        {"x", place.value("x").toDouble()},
                        {"y", place.value("y").toDouble()},
                        {"width", place.value("width").toDouble()},
                        {"height", place.value("height").toDouble()},
                        {"lastUsed", window.value("last-focus-timestamp").toDouble()},
                        {"desktop", desktop},
                        {"picture", QStringLiteral("image://window-picture/%1?%2").arg(path).arg(this_overview)},
                    });
                }

                overviewPicturesAwaited -= 1;
                if (overviewPicturesAwaited == 0)
                {
                    // Most recently used last, so it is drawn on top.
                    std::sort(overviewWindows.begin(), overviewWindows.end(),
                        [](const QVariant& first, const QVariant& second) {
                        return first.toMap().value("lastUsed").toDouble() < second.toMap().value("lastUsed").toDouble();
                    });
                    emit overviewTaken(overviewWindows, overviewDesktops);
                }
            });
        }

        if (overviewPicturesAwaited == 0)
        {
            emit overviewTaken(overviewWindows, overviewDesktops);
        }
    });
}

bool WindowPictures::bringBack(const QString& appId, const QString& title, int rank, int slideMs)
{
    const int id = find(appId, title, rank);
    if (id == -1)
    {
        return false;
    }

    compositor.request(
        "window-rules/view-info", QJsonObject{{"id", id}}, [this, id, slideMs](const QJsonObject& answer) {
        const QJsonObject window = answer.value("info").toObject();
        const QJsonObject place = window.value("geometry").toObject();
        const int screen = window.value("output-id").toInt(-1);

        compositor.request("window-rules/output-info", QJsonObject{{"id", screen}},
            [this, id, slideMs, place, screen](const QJsonObject& output) {
            const double screen_width = output.value("geometry").toObject().value("width").toDouble();
            const QJsonObject shown = output.value("workspace").toObject();
            const double middle = place.value("x").toDouble() + place.value("width").toDouble() / 2;
            const int desktops_away = screen_width > 0 ? int(std::floor(middle / screen_width)) : 0;
            if (desktops_away == 0)
            {
                focusWindow(id);
                return;
            }

            const QJsonObject where{{"x", shown.value("x").toInt() + desktops_away}, {"y", shown.value("y").toInt()},
                {"output-id", screen}};
            compositor.request("vswitch/set-workspace", where, [this, id, slideMs](const QJsonObject&) {
                // Let the desktop slide come to rest first.
                const int rest_ms = 50;
                QTimer::singleShot(slideMs + rest_ms, this, [this, id]() { focusWindow(id); });
            });
        });
    });
    return true;
}

void WindowPictures::focusWindow(int id)
{
    compositor.request("window-rules/focus-view", QJsonObject{{"id", id}}, [](const QJsonObject&) {});
}
