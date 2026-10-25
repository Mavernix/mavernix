// "image://window-picture/<path>": a raw window picture written by the
// compositor (src/compositor/window-pictures/). Format: a text line
// "WINDOWPICTURE width height", then RGBA8888 rows.

#pragma once

#include <QQuickImageProvider>

class WindowPictureProvider : public QQuickImageProvider
{
  public:
    WindowPictureProvider();

    // `id` is the path; anything after "?" is ignored and only busts QML's cache.
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
