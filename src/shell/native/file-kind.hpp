// Icon names for a file known only by its path.

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
};
