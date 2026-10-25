#include "folder-watch.hpp"

#include <QDir>
#include <QFileInfo>

FolderWatch::FolderWatch(QObject* parent) : QObject(parent)
{
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &FolderWatch::look);
}

QString FolderWatch::path() const
{
    return watchedPath;
}

void FolderWatch::setPath(const QString& path)
{
    if (path == watchedPath)
    {
        return;
    }

    watchedPath = path;
    emit pathChanged();
    look();
}

bool FolderWatch::isEmpty() const
{
    return empty;
}

void FolderWatch::look()
{
    // A missing folder: watch the nearest existing parent to see it appear.
    if (!watcher.directories().isEmpty())
    {
        watcher.removePaths(watcher.directories());
    }

    QString existing = watchedPath;
    while (!existing.isEmpty() && !QFileInfo(existing).isDir())
    {
        const QString above = QFileInfo(existing).path();
        if (above == existing)
        {
            break;
        }
        existing = above;
    }
    if (QFileInfo(existing).isDir())
    {
        watcher.addPath(existing);
    }

    const bool folderExists = QFileInfo(watchedPath).isDir();
    const bool nowEmpty =
        !folderExists ||
        QDir(watchedPath).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    if (nowEmpty != empty)
    {
        empty = nowEmpty;
        emit emptyChanged();
    }
}
