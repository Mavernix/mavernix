// The names of the icons the current icon theme offers, for the icon chooser.

#pragma once

#include <QStringList>

// Every icon name QIcon::fromTheme can find in the current icon theme, its
// parents and hicolor, once each: the current theme's own first, then each
// theme's in the order Qt searches them, each theme's sorted.
QStringList icon_theme_names();
