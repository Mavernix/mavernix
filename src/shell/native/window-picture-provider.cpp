#include "window-picture-provider.hpp"

#include <QFile>

namespace
{
const QByteArray FIRST_WORD = "WINDOWPICTURE";
const int BYTES_PER_PIXEL = 4;
// Keeps a damaged file from asking for all memory.
const int LARGEST_SIDE = 16384;
} // namespace

WindowPictureProvider::WindowPictureProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage WindowPictureProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
    const QString path = id.section(QLatin1Char('?'), 0, 0);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning().noquote() << "Shell.Native: no window picture at" << path;
        return {};
    }

    const QList<QByteArray> words = file.readLine().trimmed().split(' ');
    const int width = words.value(1).toInt();
    const int height = words.value(2).toInt();
    const bool sound = (words.value(0) == FIRST_WORD) && (width > 0) && (height > 0) && (width <= LARGEST_SIDE) &&
                       (height <= LARGEST_SIDE);
    const QByteArray pixels = sound ? file.read(qint64(width) * height * BYTES_PER_PIXEL) : QByteArray();
    if (!sound || (pixels.size() != qsizetype(width) * height * BYTES_PER_PIXEL))
    {
        qWarning().noquote() << "Shell.Native:" << path << "is not a whole window picture";
        return {};
    }

    // A copy: the image would otherwise point into `pixels`.
    QImage picture = QImage(reinterpret_cast<const uchar*>(pixels.constData()), width, height, width * BYTES_PER_PIXEL,
        QImage::Format_RGBA8888)
                         .copy();
    if (size != nullptr)
    {
        *size = picture.size();
    }
    if (requestedSize.isValid() && !requestedSize.isEmpty())
    {
        picture = picture.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return picture;
}
