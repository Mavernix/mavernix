#include "icon-theme-names.hpp"

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSet>
#include <QSettings>

#include <algorithm>

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
    QStringList listed;
    QSet<QString> seen;
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
        QSet<QString> names;
        // A theme's parents are searched before the fallbacks queued above.
        to_visit = read_theme(theme, names) + to_visit;

        // Sea Lion draws a hundred icons and inherits Breeze's thousands:
        // sorted all together, its own would be lost among them.
        QStringList own;
        for (const QString& name : names)
        {
            if (!seen.contains(name))
            {
                seen.insert(name);
                own.append(name);
            }
        }
        own.sort(Qt::CaseInsensitive);
        listed += own;
    }
    return listed;
}

QList<InstalledIconTheme> installed_icon_themes()
{
    QList<InstalledIconTheme> themes;
    QSet<QString> seen;
    // Earlier search paths win, as they do when Qt looks a theme up.
    for (const QString& search_path : QIcon::themeSearchPaths())
    {
        const QDir folder(search_path);
        for (const QString& theme : folder.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            const QString index_file = folder.filePath(theme + QStringLiteral("/index.theme"));
            if (seen.contains(theme) || theme == last_theme || theme == QLatin1String("default") ||
                !QFileInfo::exists(index_file))
            {
                continue;
            }
            const QSettings index(index_file, QSettings::IniFormat);
            // A theme of mouse pointers alone lists no icon folders; an empty
            // "Directories=" comes back as one empty name. Hidden=true is not
            // heeded: Adwaita, GNOME's own icons, says it.
            QStringList icon_folders = index.value(QStringLiteral("Icon Theme/Directories")).toStringList();
            icon_folders.removeAll(QString());
            if (icon_folders.isEmpty())
            {
                continue;
            }
            seen.insert(theme);
            // A name with a comma comes back as a list.
            const QString name =
                index.value(QStringLiteral("Icon Theme/Name")).toStringList().join(QStringLiteral(", "));
            themes.append({theme, name.isEmpty() ? theme : name});
        }
    }
    std::sort(themes.begin(), themes.end(), [](const InstalledIconTheme& one, const InstalledIconTheme& other) {
        return one.name.compare(other.name, Qt::CaseInsensitive) < 0;
    });
    return themes;
}
