#include "installed-programs.hpp"

#include <QRegularExpression>

#include <algorithm>

// GLib defines `signals` as a struct member's name, which Qt's own
// `signals` keyword would spoil; this include must not see the keyword.
#undef signals
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>

// The picture of a program: its icon by name from the icon theme, or a file.
static QIcon icon_of(GAppInfo* program)
{
    GIcon* icon = g_app_info_get_icon(program);
    if (icon == nullptr)
    {
        return QIcon();
    }
    if (G_IS_THEMED_ICON(icon))
    {
        const gchar* const* names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        for (int index = 0; names != nullptr && names[index] != nullptr; index++)
        {
            const QIcon found = QIcon::fromTheme(QString::fromUtf8(names[index]));
            if (!found.isNull())
            {
                return found;
            }
        }
    }
    if (G_IS_FILE_ICON(icon))
    {
        GFile* file = g_file_icon_get_file(G_FILE_ICON(icon));
        char* path = g_file_get_path(file);
        const QIcon from_file = path != nullptr ? QIcon(QString::fromUtf8(path)) : QIcon();
        g_free(path);
        return from_file;
    }
    return QIcon();
}

// A desktop entry's command line has places for the files to open ("%U",
// "%f", ...); a shortcut starts the program with none.
static QString without_file_places(const QString& command_line)
{
    static const QRegularExpression places(QStringLiteral("\\s*%[fFuUdDnNickvm]"));
    return QString(command_line).remove(places).trimmed();
}

static InstalledProgram describe(GAppInfo* program)
{
    InstalledProgram described;
    described.id = QString::fromUtf8(g_app_info_get_id(program));
    described.name = QString::fromUtf8(g_app_info_get_name(program));
    const char* command_line = g_app_info_get_commandline(program);
    described.command = command_line != nullptr ? without_file_places(QString::fromUtf8(command_line)) : QString();
    described.icon = icon_of(program);
    return described;
}

static void sort_by_name(QList<InstalledProgram>& programs)
{
    std::sort(programs.begin(), programs.end(), [](const InstalledProgram& first, const InstalledProgram& second) {
        return first.name.localeAwareCompare(second.name) < 0;
    });
}

QList<InstalledProgram> installed_programs()
{
    QList<InstalledProgram> programs;
    GList* every = g_app_info_get_all();
    for (GList* entry = every; entry != nullptr; entry = entry->next)
    {
        GAppInfo* program = G_APP_INFO(entry->data);
        // Those meant to be seen in menus; the others are helpers.
        if (g_app_info_should_show(program))
        {
            programs.append(describe(program));
        }
    }
    g_list_free_full(every, g_object_unref);
    sort_by_name(programs);
    return programs;
}

QList<InstalledProgram> programs_for_kind(const QString& kind)
{
    QList<InstalledProgram> programs;
    GList* registered = g_app_info_get_all_for_type(kind.toUtf8().constData());
    for (GList* entry = registered; entry != nullptr; entry = entry->next)
    {
        const InstalledProgram described = describe(G_APP_INFO(entry->data));
        const bool known = std::any_of(programs.begin(), programs.end(),
            [&described](const InstalledProgram& other) { return other.id == described.id; });
        if (!known)
        {
            programs.append(described);
        }
    }
    g_list_free_full(registered, g_object_unref);
    sort_by_name(programs);
    return programs;
}

QString usual_program_id(const QString& kind)
{
    // (With the second argument FALSE, a program that would only open the
    // kind by way of a kind it is a part of is not named.)
    GAppInfo* usual = g_app_info_get_default_for_type(kind.toUtf8().constData(), FALSE);
    if (usual == nullptr)
    {
        return QString();
    }
    const QString id = QString::fromUtf8(g_app_info_get_id(usual));
    g_object_unref(usual);
    return id;
}

QString make_usual_program(const QString& id, const QStringList& kinds)
{
    GDesktopAppInfo* program = g_desktop_app_info_new(id.toUtf8().constData());
    if (program == nullptr)
    {
        return QStringLiteral("There is no program \"%1\" on this computer.").arg(id);
    }

    QString problem;
    for (const QString& kind : kinds)
    {
        // GIO writes this into ~/.config/mimeapps.list, where every program
        // that asks for the usual program finds it.
        GError* error = nullptr;
        if (!g_app_info_set_as_default_for_type(G_APP_INFO(program), kind.toUtf8().constData(), &error))
        {
            problem = QStringLiteral("\"%1\" could not be made the usual program for %2: %3")
                          .arg(id, kind, QString::fromUtf8(error->message));
            g_error_free(error);
            break;
        }
    }

    g_object_unref(program);
    return problem;
}
