#include "icon-files.hpp"

#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QtGui/private/qiconloader_p.h>

#include <cstring>

// GLib uses `signals` as a member name, which Qt's keyword macro breaks.
#undef signals
#include <cairo.h>
#include <librsvg/rsvg.h>

static const int default_side = 128;

static bool is_drawing(const QString& file)
{
    return file.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive) ||
           file.endsWith(QLatin1String(".svgz"), Qt::CaseInsensitive);
}

// A file that cannot be drawn gives a transparent image and a warning.
static QImage draw_with_librsvg(const QString& file, int side)
{
    QImage drawn(side, side, QImage::Format_ARGB32_Premultiplied);
    drawn.fill(Qt::transparent);

    GError* error = nullptr;
    RsvgHandle* drawing = rsvg_handle_new_from_file(file.toLocal8Bit().constData(), &error);
    if (drawing == nullptr)
    {
        qWarning("svg-icon: cannot read %s: %s", qPrintable(file), error->message);
        g_error_free(error);
        return drawn;
    }

    // cairo's ARGB32 has the same layout as Qt's premultiplied ARGB.
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, side, side);
    cairo_t* painter = cairo_create(surface);
    const RsvgRectangle whole{0, 0, double(side), double(side)};
    if (!rsvg_handle_render_document(drawing, painter, &whole, &error))
    {
        qWarning("svg-icon: cannot draw %s: %s", qPrintable(file), error->message);
        g_error_free(error);
    }
    cairo_destroy(painter);
    cairo_surface_flush(surface);

    const unsigned char* rows = cairo_image_surface_get_data(surface);
    const int row_bytes = cairo_image_surface_get_stride(surface);
    for (int row = 0; row < side; row++)
    {
        std::memcpy(drawn.scanLine(row), rows + row * row_bytes, size_t(side) * 4);
    }
    cairo_surface_destroy(surface);
    g_object_unref(drawing);
    return drawn;
}

static const int settle_ms = 1500;

IconFiles::IconFiles(QObject* parent) : QObject(parent)
{
    settle.setSingleShot(true);
    settle.setInterval(settle_ms);
    connect(&settle, &QTimer::timeout, this, &IconFiles::foldersChanged);
    connect(&folders, &QFileSystemWatcher::directoryChanged, &settle, qOverload<>(&QTimer::start));
    connect(&folders, &QFileSystemWatcher::fileChanged, &settle, qOverload<>(&QTimer::start));
    watchFolders();
}

int IconFiles::revision() const
{
    return currentRevision;
}

void IconFiles::watchFolders()
{
    // Installing an icon rewrites icon-theme.cache in the theme's folder.
    QStringList wanted;
    for (const QString& folder : QIcon::themeSearchPaths())
    {
        wanted << folder;
        for (const QString& theme : {QIcon::themeName(), QStringLiteral("hicolor")})
        {
            wanted << folder + QLatin1Char('/') + theme;
        }
    }
    for (const QString& folder : wanted)
    {
        if (QFileInfo(folder).isDir() && !folders.directories().contains(folder))
        {
            folders.addPath(folder);
        }
    }
}

void IconFiles::foldersChanged()
{
    // Setting the search paths, even unchanged, makes Qt drop its cache.
    QIcon::setThemeSearchPaths(QIcon::themeSearchPaths());
    found.clear();
    watchFolders();
    currentRevision += 1;
    emit revisionChanged();
}

QString IconFiles::sourceFor(const QString& name, int revision)
{
    if (name.isEmpty())
    {
        return QString();
    }

    if (QIcon::themeName() != forTheme)
    {
        found.clear();
        forTheme = QIcon::themeName();
    }
    const auto known = found.constFind(name);
    if (known != found.constEnd())
    {
        return known.value();
    }

    // If every size is an SVG, one file serves all sizes.
    QString drawing;
    bool has_icon = false;
    if (name.startsWith(QLatin1Char('/')))
    {
        has_icon = QFileInfo::exists(name);
        drawing = is_drawing(name) ? name : QString();
    }
    else
    {
        const QThemeIconInfo info = QIconLoader::instance()->loadIcon(name);
        has_icon = !info.entries.empty();
        for (const auto& entry : info.entries)
        {
            if (!is_drawing(entry->filename))
            {
                drawing.clear();
                break;
            }
            drawing = entry->filename;
        }
    }

    QString address;
    if (has_icon && !drawing.isEmpty())
    {
        address = QStringLiteral("image://svg-icon/") + drawing;
    }
    else if (has_icon)
    {
        address = QStringLiteral("image://theme-icon/%1/%2").arg(revision).arg(name);
    }
    found.insert(name, address);
    return address;
}

ThemeIconProvider::ThemeIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap ThemeIconProvider::requestPixmap(const QString& address, QSize* size, const QSize& requested_size)
{
    // "<revision>/<name>"; a name may be a file's path, with slashes.
    const QString name = address.section(QLatin1Char('/'), 1);
    // Items ask once before they have a size.
    const QSize wanted = (requested_size.width() > 0 && requested_size.height() > 0)
                             ? requested_size
                             : QSize(default_side, default_side);
    const QPixmap picture = name.startsWith(QLatin1Char('/')) ? QIcon(name).pixmap(wanted) : themePicture(name, wanted);
    if (size != nullptr)
    {
        *size = picture.size();
    }
    return picture;
}

QPixmap ThemeIconProvider::themePicture(const QString& name, const QSize& wanted)
{
    // Not QIcon::pixmap: it multiplies the size (already in device pixels)
    // by the screen scale, picks another file, and draws SVGs with Qt's
    // reader, which ignores clip paths.
    const QThemeIconInfo info = QIconLoader::instance()->loadIcon(name);
    const QIconLoaderEngineEntry* entry = QIconLoaderEngine::entryForSize(info, wanted, 1);
    if (entry == nullptr)
    {
        // E.g. an icon only a platform theme knows.
        return QIcon::fromTheme(name).pixmap(wanted);
    }
    if (is_drawing(entry->filename))
    {
        return QPixmap::fromImage(draw_with_librsvg(entry->filename, qMax(wanted.width(), wanted.height())));
    }
    const QPixmap picture(entry->filename);
    if (picture.isNull() || picture.size() == wanted)
    {
        return picture;
    }
    return picture.scaled(wanted, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

SvgIconProvider::SvgIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage SvgIconProvider::requestImage(const QString& path, QSize* size, const QSize& requested_size)
{
    // Qt passes the path without its leading slash.
    const QString file = path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path;
    const int asked = qMax(requested_size.width(), requested_size.height());
    const QImage drawn = draw_with_librsvg(file, asked > 0 ? asked : default_side);
    if (size != nullptr)
    {
        *size = drawn.size();
    }
    return drawn;
}
