// The names of the icons the current icon theme offers, for the icon chooser.

#pragma once

#include <QStringList>

// Every icon name QIcon::fromTheme can find in the current icon theme, its
// parents and hicolor, once each, sorted.
QStringList icon_theme_names();
