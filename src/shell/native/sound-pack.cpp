#include "sound-pack.hpp"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

SoundPack::SoundPack(QObject* parent) : QObject(parent) {}

QString SoundPack::name() const
{
    return packName;
}

void SoundPack::setName(const QString& name)
{
    if (name == packName)
    {
        return;
    }

    packName = name;
    emit nameChanged();
    look();
}

QString SoundPack::folder() const
{
    return packFolder;
}

QVariantMap SoundPack::sounds() const
{
    return found;
}

void SoundPack::look()
{
    packFolder.clear();
    found.clear();

    // A name with a slash would lead out of the folders packs are kept in.
    if (!packName.isEmpty() && !packName.contains(QLatin1Char('/')))
    {
        const QStringList data_folders = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        for (const QString& data_folder : data_folders)
        {
            const QDir candidate(data_folder + QStringLiteral("/sounds/") + packName);
            if (candidate.exists(QStringLiteral("stereo")))
            {
                packFolder = candidate.absolutePath();
                break;
            }
        }
    }

    if (!packFolder.isEmpty())
    {
        // In order of preference when an event has several files.
        const QStringList kinds = {QStringLiteral("wav"), QStringLiteral("oga"), QStringLiteral("ogg")};
        const QDir sounds_folder(packFolder + QStringLiteral("/stereo"));
        for (const QString& kind : kinds)
        {
            const QFileInfoList files =
                sounds_folder.entryInfoList({QStringLiteral("*.") + kind}, QDir::Files, QDir::Name);
            for (const QFileInfo& file : files)
            {
                if (!found.contains(file.completeBaseName()))
                {
                    found.insert(file.completeBaseName(), file.absoluteFilePath());
                }
            }
        }
    }

    emit soundsChanged();
}
