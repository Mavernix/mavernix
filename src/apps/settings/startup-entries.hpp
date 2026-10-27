// The programs started at login: the desktop entries in the autostart
// folders (~/.config/autostart, then each of XDG_CONFIG_DIRS), as `dex`
// reads them in src/session/at_login.py. An entry in the user's folder takes
// the place of one of the same name further down; `Hidden=true` in it
// switches the program off.

#pragma once

#include <QIcon>
#include <QList>
#include <QString>

struct StartupEntry
{
    // "nm-applet.desktop".
    QString file_name;
    QString name;
    QIcon icon;
    bool on = true;
    // Only the user's folder has it, so it can be removed rather than
    // switched off.
    bool users_own = false;
};

// Those that would start in this desktop, by name. Helpers that menus do
// not show (NoDisplay) and programs not installed (TryExec) are left out.
QList<StartupEntry> startup_entries();

// Each returns what went wrong, else empty.
QString switch_startup_entry(const QString& file_name, bool on);
// Starts an installed program ("org.kde.kate.desktop") at login. Its entry
// is copied, or the one already there switched on.
QString add_startup_program(const QString& program_id);
QString remove_startup_entry(const QString& file_name);
