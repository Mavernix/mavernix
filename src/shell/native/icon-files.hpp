// Theme icons drawn with librsvg. Qt's SVG reader lacks masks, filters and
// clip paths, which many application icons use. Which files an icon has is
// asked of QIconLoader, a private Qt class, on the main thread only: it is
// not thread-safe, and Qt runs the image providers below on threads of its
// own for images loaded asynchronously.
//
// Qt reads the icon folders only once, so they are watched here: on a change
// Qt rereads them and `revision` counts up.
//
// Usage in QML:
//   IconFiles { id: iconFiles }
//   source: iconFiles.sourceFor(name, iconFiles.revision) || Quickshell.iconPath(name, "fallback-name")

#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QQuickImageProvider>
#include <QString>
#include <QTimer>

class IconFiles : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

  public:
    explicit IconFiles(QObject* parent = nullptr);

    int revision() const;

    // An Image source for the icon, or "" if the theme has none. `name` may
    // be a file path. Pass `revision` so the binding updates with it.
    Q_INVOKABLE QString sourceFor(const QString& name, int revision);

    // Whether the icon theme in use has this icon itself, rather than a
    // theme it inherits (hicolor, where programs put their own).
    Q_INVOKABLE bool isThemeOwn(const QString& name, int revision);

  signals:
    void revisionChanged();

  private:
    void watchFolders();
    void foldersChanged();

    // Cache for the icon theme named in `forTheme`.
    QHash<QString, QString> found;
    QString forTheme;
    QFileSystemWatcher folders;
    // Waits for an installation's many changes to settle.
    QTimer settle;
    int currentRevision = 0;
};

// "image://theme-icon/<revision>/<name>", for themes that also ship PNGs.
// Only for names IconFiles::sourceFor has looked up.
class ThemeIconProvider : public QQuickImageProvider
{
  public:
    ThemeIconProvider();
    QImage requestImage(const QString& address, QSize* size, const QSize& requested_size) override;

    // The theme's icon, or the file if `name` is a path.
    static QImage picture(const QString& name, const QSize& wanted);
};

// "image://fitted-icon/<fill>/<revision>/<name>": the icon, shrunk around its
// middle if its shape spans more than `fill` (0 to 1) of its square. Icons
// that fill their whole square look bigger next to a theme's, which leave a
// margin.
class FittedIconProvider : public QQuickImageProvider
{
  public:
    FittedIconProvider();
    QImage requestImage(const QString& address, QSize* size, const QSize& requested_size) override;
};

// "image://svg-icon/<path>": an SVG file drawn with librsvg.
class SvgIconProvider : public QQuickImageProvider
{
  public:
    SvgIconProvider();
    QImage requestImage(const QString& path, QSize* size, const QSize& requested_size) override;
};
