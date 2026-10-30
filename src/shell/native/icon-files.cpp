#include "icon-files.hpp"

#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QMutex>
#include <QPainter>
#include <QtGui/private/qiconloader_p.h>

#include <cstring>
#include <optional>

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

// --- The files of each icon ------------------------------------------------------
//
// Found by IconFiles::sourceFor on the main thread, and read by the
// providers on whichever thread Qt runs them. Shared by every IconFiles, so
// never emptied (another one's address may still need an entry): a lookup
// after a change of theme or of the icon folders replaces the entry.

namespace
{
struct IconFile
{
    QString path;
    // The side it is drawn at, in pixels; 0 for a drawing, good at any size.
    int side = 0;
};

QMutex files_lock;
QHash<QString, QList<IconFile>> files_by_name;

void remember_files(const QString& name, const QThemeIconInfo& info)
{
    QList<IconFile> files;
    for (const auto& entry : info.entries)
    {
        const bool scalable = entry->dir.type == QIconDirInfo::Scalable || is_drawing(entry->filename);
        files.append({entry->filename, scalable ? 0 : entry->dir.size * entry->dir.scale});
    }
    const QMutexLocker locker(&files_lock);
    files_by_name.insert(name, files);
}

// The file to draw at `side`: one of exactly that size, else a drawing, else
// the smallest one larger (shrinking keeps it sharp), else the largest. Null
// if the icon was never looked up.
std::optional<IconFile> best_file(const QString& name, int side)
{
    QList<IconFile> files;
    {
        const QMutexLocker locker(&files_lock);
        files = files_by_name.value(name);
    }
    std::optional<IconFile> drawing, larger, largest;
    for (const IconFile& file : files)
    {
        if (file.side == side)
        {
            return file;
        }
        if (file.side == 0)
        {
            drawing = file;
        }
        else if (file.side > side && (!larger || file.side < larger->side))
        {
            larger = file;
        }
        else if (!largest || file.side > largest->side)
        {
            largest = file;
        }
    }
    return drawing ? drawing : (larger ? larger : largest);
}

// A picture file at `wanted` size, keeping its proportions. QImage, not
// QPixmap: QPixmap::load goes through QPixmapCache, which is for the main
// thread only.
QImage read_scaled(const QString& path, const QSize& wanted)
{
    const QImage read(path);
    if (read.isNull() || read.size() == wanted)
    {
        return read;
    }
    return read.scaled(wanted, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
} // namespace

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
        remember_files(name, info);
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

bool IconFiles::isThemeOwn(const QString& name, int)
{
    if (name.isEmpty() || name.startsWith(QLatin1Char('/')))
    {
        return false;
    }
    const QString folder = QLatin1Char('/') + QIcon::themeName() + QLatin1Char('/');
    const QThemeIconInfo info = QIconLoader::instance()->loadIcon(name);
    for (const auto& entry : info.entries)
    {
        if (!entry->filename.contains(folder))
        {
            return false;
        }
    }
    return !info.entries.empty();
}

ThemeIconProvider::ThemeIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage ThemeIconProvider::requestImage(const QString& address, QSize* size, const QSize& requested_size)
{
    // "<revision>/<name>"; a name may be a file's path, with slashes.
    const QString name = address.section(QLatin1Char('/'), 1);
    // Items ask once before they have a size.
    const QSize wanted = (requested_size.width() > 0 && requested_size.height() > 0)
                             ? requested_size
                             : QSize(default_side, default_side);
    const QImage drawn = picture(name, wanted);
    if (size != nullptr)
    {
        *size = drawn.size();
    }
    return drawn;
}

QImage ThemeIconProvider::picture(const QString& name, const QSize& wanted)
{
    const int side = qMax(wanted.width(), wanted.height());
    QString path = name;
    if (!name.startsWith(QLatin1Char('/')))
    {
        const std::optional<IconFile> file = best_file(name, side);
        if (!file)
        {
            return QImage();
        }
        path = file->path;
    }
    // Not Qt's SVG reader, which ignores clip paths.
    return is_drawing(path) ? draw_with_librsvg(path, side) : read_scaled(path, wanted);
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

FittedIconProvider::FittedIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

// The side of the smallest square around the parts at least this opaque.
static const int shape_alpha = 24;

static int shape_side(const QImage& image)
{
    int left = image.width(), right = -1, top = image.height(), bottom = -1;
    for (int y = 0; y < image.height(); y++)
    {
        const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x++)
        {
            if (qAlpha(row[x]) >= shape_alpha)
            {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    return right < 0 ? 0 : qMax(right - left + 1, bottom - top + 1);
}

QImage FittedIconProvider::requestImage(const QString& address, QSize* size, const QSize& requested_size)
{
    // "<fill>/<revision>/<name>"; a name may be a file's path, with slashes.
    const double fill = address.section(QLatin1Char('/'), 0, 0).toDouble();
    const QString name = address.section(QLatin1Char('/'), 2);
    const int side = qMax(requested_size.width(), requested_size.height()) > 0
                         ? qMax(requested_size.width(), requested_size.height())
                         : default_side;
    QImage drawn =
        ThemeIconProvider::picture(name, QSize(side, side)).convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const int shape = shape_side(drawn);
    const int square = qMax(drawn.width(), drawn.height());
    if ((fill > 0) && (shape > 0) && (square > 0) && (double(shape) / square > fill))
    {
        const double shrink = fill * square / shape;
        QImage fitted(drawn.size(), QImage::Format_ARGB32_Premultiplied);
        fitted.fill(Qt::transparent);
        QPainter painter(&fitted);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QSizeF smaller = QSizeF(drawn.size()) * shrink;
        const QPointF corner((drawn.width() - smaller.width()) / 2, (drawn.height() - smaller.height()) / 2);
        painter.drawImage(QRectF(corner, smaller), drawn);
        painter.end();
        drawn = fitted;
    }
    if (size != nullptr)
    {
        *size = drawn.size();
    }
    return drawn;
}
