// The applications installed on this computer, as their desktop entries
// describe them: what the App Icons pane lists.

#pragma once

#include <QList>
#include <QString>

struct InstalledApplication
{
    // The file's name without ".desktop" ("org.kde.dolphin"), as the shell
    // names applications; the key for the user's own icon.
    QString id;
    // In the user's language.
    QString name;
    // The entry's `Icon` line: a theme icon name or a file path.
    QString icon;
};

// The applications a menu would show (not those that hide), by name.
QList<InstalledApplication> installed_applications();
