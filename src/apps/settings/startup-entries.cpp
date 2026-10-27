#include "startup-entries.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>

#include <algorithm>

// GLib defines `signals` as a struct member's name, which Qt's own
// `signals` keyword would spoil; this include must not see the keyword.
#undef signals
#include <gio/gdesktopappinfo.h>
#include <glib.h>

static const char* const ENTRY_GROUP = "Desktop Entry";

static QString users_folder()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
}

// The user's folder first, then the system's, as dex looks.
static QStringList folders()
{
    QStringList found = {users_folder()};
    const QString config_dirs = qEnvironmentVariable("XDG_CONFIG_DIRS", QStringLiteral("/etc/xdg"));
    for (const QString& dir : config_dirs.split(QLatin1Char(':'), Qt::SkipEmptyParts))
    {
        found << dir + QStringLiteral("/autostart");
    }
    return found;
}

// Every file of this name, the one that counts first.
static QStringList paths_of(const QString& file_name)
{
    QStringList paths;
    for (const QString& folder : folders())
    {
        const QString path = folder + QLatin1Char('/') + file_name;
        if (QFileInfo::exists(path))
        {
            paths << path;
        }
    }
    return paths;
}

// Null if it cannot be read; free with g_key_file_free().
static GKeyFile* read_entry(const QString& path)
{
    GKeyFile* entry = g_key_file_new();
    const auto flags = static_cast<GKeyFileFlags>(G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS);
    if (!g_key_file_load_from_file(entry, path.toUtf8().constData(), flags, nullptr))
    {
        g_key_file_free(entry);
        return nullptr;
    }
    return entry;
}

static bool flag(GKeyFile* entry, const char* key)
{
    // A missing key or one that is not a boolean is false.
    return g_key_file_get_boolean(entry, ENTRY_GROUP, key, nullptr);
}

static QString text(GKeyFile* entry, const char* key, bool translated = false)
{
    gchar* value = translated ? g_key_file_get_locale_string(entry, ENTRY_GROUP, key, nullptr, nullptr)
                              : g_key_file_get_string(entry, ENTRY_GROUP, key, nullptr);
    const QString read = QString::fromUtf8(value);
    g_free(value);
    return read;
}

static QStringList list(GKeyFile* entry, const char* key)
{
    QStringList read;
    gchar** values = g_key_file_get_string_list(entry, ENTRY_GROUP, key, nullptr, nullptr);
    for (int index = 0; values != nullptr && values[index] != nullptr; index++)
    {
        read << QString::fromUtf8(values[index]);
    }
    g_strfreev(values);
    return read;
}

static QString save(GKeyFile* entry, const QString& path)
{
    QDir().mkpath(QFileInfo(path).path());
    GError* error = nullptr;
    if (!g_key_file_save_to_file(entry, path.toUtf8().constData(), &error))
    {
        const QString problem = QString::fromUtf8(error->message);
        g_error_free(error);
        return problem;
    }
    return QString();
}

// Whether dex would start it in this desktop, `Hidden` aside.
static bool starts_here(GKeyFile* entry)
{
    if (flag(entry, "NoDisplay"))
    {
        return false;
    }

    // dex is given the whole XDG_CURRENT_DESKTOP.
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QStringList only_in = list(entry, "OnlyShowIn");
    if (!only_in.isEmpty() && !only_in.contains(desktop))
    {
        return false;
    }
    if (list(entry, "NotShowIn").contains(desktop))
    {
        return false;
    }

    const QString try_exec = text(entry, "TryExec");
    if (!try_exec.isEmpty())
    {
        const bool found = QFileInfo(try_exec).isAbsolute() ? QFileInfo(try_exec).isExecutable()
                                                            : !QStandardPaths::findExecutable(try_exec).isEmpty();
        if (!found)
        {
            return false;
        }
    }
    return true;
}

QList<StartupEntry> startup_entries()
{
    QStringList file_names;
    for (const QString& folder : folders())
    {
        for (const QString& file_name : QDir(folder).entryList({QStringLiteral("*.desktop")}, QDir::Files))
        {
            if (!file_names.contains(file_name))
            {
                file_names << file_name;
            }
        }
    }

    QList<StartupEntry> entries;
    for (const QString& file_name : file_names)
    {
        const QStringList paths = paths_of(file_name);
        GKeyFile* first = read_entry(paths.first());
        if (first == nullptr)
        {
            continue;
        }

        StartupEntry described;
        described.file_name = file_name;
        described.on = !flag(first, "Hidden");
        described.users_own = paths.size() == 1 && paths.first().startsWith(users_folder() + QLatin1Char('/'));

        // A switched-off copy may say nothing else: the name and the rest
        // are the first entry's that is not switched off.
        GKeyFile* details = first;
        for (int index = 1; !described.on && index < paths.size(); index++)
        {
            GKeyFile* below = read_entry(paths[index]);
            if (below != nullptr && !flag(below, "Hidden"))
            {
                details = below;
                break;
            }
            if (below != nullptr)
            {
                g_key_file_free(below);
            }
        }

        if (starts_here(details))
        {
            described.name = text(details, "Name", true);
            if (described.name.isEmpty())
            {
                described.name = QFileInfo(file_name).completeBaseName();
            }
            const QString icon = text(details, "Icon");
            described.icon = QFileInfo(icon).isAbsolute() ? QIcon(icon) : QIcon::fromTheme(icon);
            entries << described;
        }

        if (details != first)
        {
            g_key_file_free(details);
        }
        g_key_file_free(first);
    }

    std::sort(entries.begin(), entries.end(),
        [](const StartupEntry& one, const StartupEntry& other) { return one.name.localeAwareCompare(other.name) < 0; });
    return entries;
}

// A copy of `path` in the user's folder, switched on or off.
static QString save_switched_copy(const QString& path, const QString& own_path, bool on)
{
    GKeyFile* entry = read_entry(path);
    if (entry == nullptr)
    {
        return QStringLiteral("%1 cannot be read").arg(path);
    }
    g_key_file_set_boolean(entry, ENTRY_GROUP, "Hidden", !on);
    const QString problem = save(entry, own_path);
    g_key_file_free(entry);
    return problem;
}

QString switch_startup_entry(const QString& file_name, bool on)
{
    const QStringList paths = paths_of(file_name);
    if (paths.isEmpty())
    {
        return QStringLiteral("%1 is gone").arg(file_name);
    }

    const QString own_path = users_folder() + QLatin1Char('/') + file_name;
    const bool has_own = paths.first() == own_path;
    // The system's entries under the user's copy, the one that counts first.
    const QStringList below = has_own ? paths.mid(1) : paths;

    if (!on)
    {
        // The user's own copy says so, made from the system's if need be.
        return save_switched_copy(paths.first(), own_path, false);
    }
    if (below.isEmpty())
    {
        // One the user added, switched off before.
        return save_switched_copy(own_path, own_path, true);
    }

    // Switched on again, the system's entry counts again as it is: a copy
    // left behind would hide its updates, or, a mere stub, the program.
    if (has_own && !QFile::remove(own_path))
    {
        return QStringLiteral("%1 cannot be removed").arg(own_path);
    }
    GKeyFile* system = read_entry(below.first());
    const bool system_is_off = system != nullptr && flag(system, "Hidden");
    if (system != nullptr)
    {
        g_key_file_free(system);
    }
    // One the system itself switches off needs a copy that says on.
    return system_is_off ? save_switched_copy(below.first(), own_path, true) : QString();
}

QString add_startup_program(const QString& program_id)
{
    if (!paths_of(program_id).isEmpty())
    {
        return switch_startup_entry(program_id, true);
    }

    GDesktopAppInfo* program = g_desktop_app_info_new(program_id.toUtf8().constData());
    if (program == nullptr)
    {
        return QStringLiteral("%1 is not installed").arg(program_id);
    }
    GKeyFile* entry = read_entry(QString::fromUtf8(g_desktop_app_info_get_filename(program)));
    g_object_unref(program);
    if (entry == nullptr)
    {
        return QStringLiteral("%1 cannot be read").arg(program_id);
    }
    const QString problem = save(entry, users_folder() + QLatin1Char('/') + program_id);
    g_key_file_free(entry);
    return problem;
}

QString remove_startup_entry(const QString& file_name)
{
    QFile own(users_folder() + QLatin1Char('/') + file_name);
    if (!own.remove())
    {
        return own.errorString();
    }
    return QString();
}
