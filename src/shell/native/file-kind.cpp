#include "file-kind.hpp"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

FileKind::FileKind(QObject* parent) : QObject(parent) {}

QVariantMap FileKind::iconsFor(const QString& path) const
{
    // By name alone: quick, and enough for an icon.
    static const QMimeDatabase kinds;
    const QMimeType kind = kinds.mimeTypeForFile(QFileInfo(path).fileName(), QMimeDatabase::MatchExtension);

    return QVariantMap{
        {QStringLiteral("icon"), kind.iconName()},
        {QStringLiteral("genericIcon"), kind.genericIconName()},
    };
}

bool FileKind::isFolder(const QString& path) const
{
    return QFileInfo(path).isDir();
}
