#include "file-moment.hpp"

QDateTime moment_for(const QFileInfo& file, const QString& order)
{
    if (order == QLatin1String("created"))
    {
        return file.birthTime();
    }
    if (order == QLatin1String("added"))
    {
        // Linux does not record when a file came into a folder; the
        // metadata change time (set by a move or download) is closest.
        return file.metadataChangeTime();
    }
    return file.lastModified();
}
