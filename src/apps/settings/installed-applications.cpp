#include "installed-applications.hpp"

#include <algorithm>

// GLib defines `signals` as a struct member's name, which Qt's own
// `signals` keyword would spoil; these includes must not see the keyword.
#undef signals
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>

// The ending of a desktop entry's file name, which an ID leaves out.
static const QLatin1String entry_file_ending(".desktop");

QList<InstalledApplication> installed_applications()
{
    QList<InstalledApplication> found;

    // GIO reads the desktop entries of every folder the system names, in
    // the order of the specification, and knows which entries are hidden.
    GList* every = g_app_info_get_all();
    for (GList* place = every; place != nullptr; place = place->next)
    {
        GAppInfo* program = G_APP_INFO(place->data);
        if (!g_app_info_should_show(program))
        {
            continue;
        }

        InstalledApplication application;
        application.id = QString::fromUtf8(g_app_info_get_id(program));
        if (application.id.endsWith(entry_file_ending))
        {
            application.id.chop(entry_file_ending.size());
        }
        application.name = QString::fromUtf8(g_app_info_get_name(program));

        // The raw `Icon` line, as the shell reads it, and not GIO's
        // picture of it: both must come to the same icon.
        if (G_IS_DESKTOP_APP_INFO(program))
        {
            const char* icon = g_desktop_app_info_get_string(G_DESKTOP_APP_INFO(program), "Icon");
            if (icon != nullptr)
            {
                application.icon = QString::fromUtf8(icon);
                g_free(const_cast<char*>(icon));
            }
        }
        found.append(application);
    }
    g_list_free_full(every, g_object_unref);

    std::sort(found.begin(), found.end(), [](const InstalledApplication& first, const InstalledApplication& second) {
        return first.name.compare(second.name, Qt::CaseInsensitive) < 0;
    });
    return found;
}
