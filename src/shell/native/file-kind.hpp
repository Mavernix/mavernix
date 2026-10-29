// Icon names for a file known only by its path, and the moment a folder's
// date orders sort it by.

#pragma once

#include <QObject>
#include <QVariantMap>

class FileKind : public QObject
{
    Q_OBJECT

  public:
    explicit FileKind(QObject* parent = nullptr);

    // { icon, genericIcon }, e.g. "text-plain" and "text-x-generic".
    Q_INVOKABLE QVariantMap iconsFor(const QString& path) const;
    // False for a path that does not exist.
    Q_INVOKABLE bool isFolder(const QString& path) const;
    // When the file was "added", "modified" or "created" (file-moment.hpp),
    // in milliseconds since 1970; 0 for a path that does not exist.
    Q_INVOKABLE double momentOf(const QString& path, const QString& order) const;
};
