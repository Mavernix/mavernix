#include "icon-theme-names.hpp"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSet>
#include <QSettings>

// Qt finds icons by the freedesktop icon theme rules but cannot list them,
// so the folders are read here by the same rules.
// https://specifications.freedesktop.org/icon-theme-spec/latest/

namespace
{

const QStringList icon_file_patterns = {
    QStringLiteral("*.png"), QStringLiteral("*.svg"), QStringLiteral("*.svgz"), QStringLiteral("*.xpm")};

// Icons whose names end so are one-colour drawings that programs recolour
// to fit beside text; as an application's icon they would be a grey glyph.
const QString symbolic_suffix = QStringLiteral("-symbolic");

// Every theme falls back to it, by the specification.
const QString last_theme = QStringLiteral("hicolor");

// Adds one theme's icon names and returns the themes it inherits from.
QStringList read_theme(const QString& theme, QSet<QString>& names)
{
    QStringList parents;
    for (const QString& search_path : QIcon::themeSearchPaths())
    {
        const QDir theme_folder(search_path + QLatin1Char('/') + theme);
        const QString index_file = theme_folder.filePath(QStringLiteral("index.theme"));
        if (!QFileInfo::exists(index_file))
        {
            continue;
        }

        // "Inherits=breeze,hicolor" comes back as a list of strings.
        const QSettings index(index_file, QSettings::IniFormat);
        for (const QString& parent : index.value(QStringLiteral("Icon Theme/Inherits")).toStringList())
        {
            if (!parents.contains(parent))
            {
                parents.append(parent);
            }
        }

        // Only the folders the index lists are searched by Qt; an icon in
        // another one would be listed and then not found.
        QStringList folders = index.value(QStringLiteral("Icon Theme/Directories")).toStringList();
        folders += index.value(QStringLiteral("Icon Theme/ScaledDirectories")).toStringList();
        for (const QString& folder : folders)
        {
            const QDir icon_folder(theme_folder.filePath(folder));
            for (const QFileInfo& file : icon_folder.entryInfoList(icon_file_patterns, QDir::Files))
            {
                // "org.kde.dolphin.svg" names the icon "org.kde.dolphin".
                const QString name = file.completeBaseName();
                if (!name.endsWith(symbolic_suffix))
                {
                    names.insert(name);
                }
            }
        }
    }
    return parents;
}

} // namespace

QStringList icon_theme_names()
{
    QSet<QString> names;
    QSet<QString> visited;
    QStringList to_visit = {QIcon::themeName(), QIcon::fallbackThemeName(), last_theme};
    while (!to_visit.isEmpty())
    {
        const QString theme = to_visit.takeFirst();
        if (theme.isEmpty() || visited.contains(theme))
        {
            continue;
        }
        visited.insert(theme);
        // A theme's parents are searched before the fallbacks queued above.
        to_visit = read_theme(theme, names) + to_visit;
    }

    QStringList sorted(names.cbegin(), names.cend());
    sorted.sort(Qt::CaseInsensitive);
    return sorted;
}
