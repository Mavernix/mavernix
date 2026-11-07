// The names of the icons the current icon theme offers, for the icon chooser.

#pragma once

#include <QList>
#include <QStringList>

// Every icon name QIcon::fromTheme can find in the current icon theme, its
// parents and hicolor, once each: the current theme's own first, then each
// theme's in the order Qt searches them, each theme's sorted.
QStringList icon_theme_names();

struct InstalledIconTheme
{
    QString folder; // what the config names it by: "breeze-dark"
    QString name;   // what its index.theme calls it: "Breeze Dark"
};

// The icon themes a user can pick, sorted by name: those with icons, neither
// hicolor nor "default" (a link to another theme).
QList<InstalledIconTheme> installed_icon_themes();

// The icon theme the user picked in another desktop, by folder name, or empty
// if they never picked one there: Plasma's from ~/.config/kdeglobals, GNOME's
// from dconf's "user" database (this session keeps its own, so it is asked
// without DCONF_PROFILE).
QString plasma_icon_theme();
QString gnome_icon_theme();
