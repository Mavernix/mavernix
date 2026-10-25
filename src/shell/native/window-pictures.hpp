// Tile pictures of minimised windows (with an optional app icon badge) and
// window pictures for the overview, written by the compositor plugin in
// src/compositor/window-pictures/.
//
// QML's window-management protocol gives windows no ID, so they are matched
// to the compositor's by app ID, title and, among equals, opening order.
//
// Usage in QML:
//   WindowPictures { id: pictures; folder: "/run/.../window-pictures"; side: 256 }
//   pictures.tileFor(window.appId, window.title, 0, "firefox")

#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "compositor-socket.hpp"

class WindowPictures : public QObject
{
    Q_OBJECT
    // Created if missing.
    Q_PROPERTY(QString folder MEMBER picturesFolder NOTIFY settingsChanged)
    // In pixels.
    Q_PROPERTY(int side MEMBER tileSide NOTIFY settingsChanged)
    // Badge size as a fraction of `side`; 0 for none.
    Q_PROPERTY(qreal badgeShare MEMBER badgeSideShare NOTIFY settingsChanged)
    // Wait until the minimise effect is over, so the capture does not stall it.
    Q_PROPERTY(int delayMs MEMBER delayAfterMinimizing NOTIFY settingsChanged)
    // Counts up whenever a picture comes or goes.
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

  public:
    explicit WindowPictures(QObject* parent = nullptr);

    int revision() const;

    // `rank` counts earlier windows with the same app ID and title;
    // `badgeIcon` is an icon theme name or a file path.
    // Returns "" while there is no picture; re-read when `revision` changes.
    Q_INVOKABLE QString tileFor(const QString& appId, const QString& title, int rank, const QString& badgeIcon);
    Q_INVOKABLE bool isComing(const QString& appId, const QString& title, int rank) const;

    // Emits `overviewTaken` with every visible window as { id, appId, title,
    // x, y, width, height, lastUsed, desktop, picture }, in screen points;
    // `picture` is an "image://window-picture/" address. Minimised windows
    // are left out.
    Q_INVOKABLE void takeOverview(int largestSide);
    Q_INVOKABLE void focusWindow(int id);

    // Restores a minimised window, first switching to its desktop: the
    // compositor alone would restore it out of sight. The window is shown
    // `slideMs` later, once the slide is over. False if unknown here.
    Q_INVOKABLE bool bringBack(const QString& appId, const QString& title, int rank, int slideMs);

  signals:
    void settingsChanged();
    void revisionChanged();
    // `desktops` is { current, count, screenWidth } of the overview's screen.
    void overviewTaken(const QVariantList& windows, const QVariantMap& desktops);

  private:
    enum class Picture
    {
        None,   // not minimised, or the compositor could not write one
        Coming, // asked for, or about to be
        Ready,
    };

    struct Window
    {
        QString appId;
        QString title;
        bool minimized = false;
        Picture picture = Picture::None;
        // Empty until the tile is composed.
        QString tileBadge;
    };

    void startWatching();
    void takeWindow(const QJsonObject& description);
    void forgetWindow(int id);
    void askForPicture(int id);
    bool composeTile(int id, const QString& badgeIcon) const;
    int find(const QString& appId, const QString& title, int rank) const;
    QString windowPicturePath(int id) const;
    QString tilePicturePath(int id) const;
    void removePictures(int id) const;
    void changed();

    CompositorSocket compositor;
    // The compositor's IDs grow with every window opened.
    QMap<int, Window> windows;

    // The overview being assembled, and each screen's desktops by screen ID.
    QVariantList overviewWindows;
    QMap<int, QVariantMap> overviewScreens;
    QVariantMap overviewDesktops;
    int overviewPicturesAwaited = 0;
    // Gives each overview's pictures new addresses.
    int overviewNumber = 0;

    QString picturesFolder;
    int tileSide = 256;
    qreal badgeSideShare = 0;
    int delayAfterMinimizing = 0;
    int currentRevision = 0;
};
