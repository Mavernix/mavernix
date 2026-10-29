#include "folder-contents.hpp"

#include "file-moment.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#include <algorithm>
#include <optional>

FolderContents::FolderContents(QObject* parent) : QObject(parent)
{
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &FolderContents::look);
}

QString FolderContents::path() const
{
    return folderPath;
}

void FolderContents::setPath(const QString& path)
{
    if (path == folderPath)
    {
        return;
    }

    folderPath = path;
    emit pathChanged();
    look();
}

QString FolderContents::order() const
{
    return wantedOrder;
}

void FolderContents::setOrder(const QString& order)
{
    if (order == wantedOrder)
    {
        return;
    }

    wantedOrder = order;
    emit orderChanged();
    look();
}

QVariantList FolderContents::entries() const
{
    return found;
}

// By name alone: quick, and enough for an icon.
static QMimeType kind_of(const QFileInfo& file)
{
    static const QMimeDatabase kinds;
    return kinds.mimeTypeForFile(file.fileName(), QMimeDatabase::MatchExtension);
}

// A launcher's name and icon from its [Desktop Entry] group. Only executable
// files count, so a downloaded launcher does nothing until the user allows it.
struct launcher_t
{
    QString name;
    QString icon;
};

static std::optional<launcher_t> launcher_in(const QFileInfo& file)
{
    if (file.suffix() != QLatin1String("desktop") || !file.isFile() || !file.isExecutable())
    {
        return std::nullopt;
    }
    QFile entry(file.absoluteFilePath());
    if (!entry.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return std::nullopt;
    }

    launcher_t launcher;
    bool in_entry_group = false;
    bool is_application = false;
    while (!entry.atEnd())
    {
        const QString line = QString::fromUtf8(entry.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('[')))
        {
            in_entry_group = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!in_entry_group)
        {
            continue;
        }
        if (line.startsWith(QLatin1String("Name=")))
        {
            launcher.name = line.mid(5);
        }
        else if (line.startsWith(QLatin1String("Icon=")))
        {
            launcher.icon = line.mid(5);
        }
        else if (line == QLatin1String("Type=Application"))
        {
            is_application = true;
        }
    }
    if (!is_application || launcher.name.isEmpty())
    {
        return std::nullopt;
    }
    return launcher;
}

void FolderContents::look()
{
    if (!watcher.directories().isEmpty())
    {
        watcher.removePaths(watcher.directories());
    }

    QFileInfoList files;
    const QDir folder(folderPath);
    if (!folderPath.isEmpty() && folder.exists())
    {
        watcher.addPath(folderPath);
        files = folder.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    }

    const QString order = wantedOrder;
    const bool by_time =
        order == QLatin1String("added") || order == QLatin1String("modified") || order == QLatin1String("created");
    std::sort(files.begin(), files.end(), [&](const QFileInfo& first, const QFileInfo& second) {
        if (by_time)
        {
            return moment_for(first, order) > moment_for(second, order);
        }
        if (order == QLatin1String("kind") && first.suffix() != second.suffix())
        {
            return first.suffix().localeAwareCompare(second.suffix()) < 0;
        }
        return first.fileName().localeAwareCompare(second.fileName()) < 0;
    });

    QVariantList entries;
    for (const QFileInfo& file : files)
    {
        const bool is_folder = file.isDir();
        if (const auto launcher = launcher_in(file))
        {
            entries.append(QVariantMap{
                {QStringLiteral("name"), launcher->name},
                {QStringLiteral("path"), file.absoluteFilePath()},
                {QStringLiteral("icon"), launcher->icon},
                {QStringLiteral("genericIcon"), QStringLiteral("application-x-executable")},
                {QStringLiteral("isFolder"), false},
                {QStringLiteral("isLauncher"), true},
            });
            continue;
        }
        const QMimeType kind = kind_of(file);
        entries.append(QVariantMap{
            {QStringLiteral("name"), file.fileName()},
            {QStringLiteral("path"), file.absoluteFilePath()},
            {QStringLiteral("icon"), is_folder ? QStringLiteral("folder") : kind.iconName()},
            {QStringLiteral("genericIcon"), is_folder ? QStringLiteral("folder") : kind.genericIconName()},
            {QStringLiteral("isFolder"), is_folder},
            {QStringLiteral("isLauncher"), false},
        });
    }

    found = entries;
    emit entriesChanged();
}
